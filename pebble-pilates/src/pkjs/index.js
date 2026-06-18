// Pilatable PebbleKit JS bridge.
//
// Responsibilities:
//  1. Open the hosted configuration page when the user taps the gear in the
//     Pebble app (showConfiguration), and persist the built session it returns.
//  2. Serve the latest session to the watch when it requests one on launch
//     (pull-on-launch model — see PLAN.md).
//
// The rich UI lives in the hosted config page (config/index.html), NOT here.

// The config page is deployed to GitHub Pages from pebble-pilates/config/
// (see .github/workflows/pages.yml). index.html is served at the site root.
var CONFIG_URL = 'https://wyoc.github.io/pilatable/';

var STORAGE_KEY = 'pilatable.session.v1';

function loadStored() {
  try {
    var raw = localStorage.getItem(STORAGE_KEY);
    if (raw) return JSON.parse(raw);
  } catch (e) {
    console.log('Pilatable: failed to parse stored session: ' + e);
  }
  // Default mirrors the watch's baked-in fallback so a fresh install syncs sanely.
  var IE = ['inhale', 'exhale'], EI = ['exhale', 'inhale'], EIE = ['exhale', 'inhale', 'exhale'];
  return {
    version: 2,
    name: 'Fundamental Mat',
    settings: { hapticsEnabled: true, intensity: 1, leadInEnabled: true },
    items: [
      { name: 'Pelvic Curl', reps: 5, movementLengthSec: 5.0, restAfterSec: 30, betweenRepsSec: 1, breathPattern: EIE },
      { name: 'Chest Lift', reps: 10, movementLengthSec: 4.0, restAfterSec: 30, betweenRepsSec: 1, breathPattern: EIE },
      { name: 'Leg Lift Supine', reps: 5, movementLengthSec: 4.0, restAfterSec: 30, betweenRepsSec: 1, breathPattern: EI },
      { name: 'Spine Twist Supine', reps: 5, movementLengthSec: 4.0, restAfterSec: 30, betweenRepsSec: 1, breathPattern: EI },
      { name: 'Chest Lift With Rotation', reps: 5, movementLengthSec: 4.0, restAfterSec: 30, betweenRepsSec: 1, breathPattern: EI },
      { name: 'Back Extension Prone', reps: 5, movementLengthSec: 5.0, restAfterSec: 30, betweenRepsSec: 1, breathPattern: EI },
      { name: 'One-Leg Circle', reps: 5, movementLengthSec: 4.0, restAfterSec: 30, betweenRepsSec: 1, breathPattern: EI },
      { name: 'Rolling Back', reps: 10, movementLengthSec: 4.0, restAfterSec: 0, betweenRepsSec: 1, breathPattern: IE }
    ]
  };
}

// Stream a session to the watch: header -> one message per item -> SYNC_DONE.
// Sequenced via the AppMessage success callback so messages don't overrun the
// outbox; chunking keeps each message well within the firmware buffer.
function sendSession(session) {
  var settings = session.settings || {};
  var header = {
    SESSION_VERSION: session.version || 1,
    SESSION_NAME: session.name || 'Session',
    ITEM_COUNT: session.items.length,
    SET_HAPTICS: settings.hapticsEnabled ? 1 : 0,
    SET_INTENSITY: typeof settings.intensity === 'number' ? settings.intensity : 1,
    SET_LEADIN: settings.leadInEnabled ? 1 : 0
  };

  function sendItem(i) {
    if (i >= session.items.length) {
      Pebble.sendAppMessage({ SYNC_DONE: 1 });
      console.log('Pilatable: session sync complete (' + session.items.length + ' items)');
      return;
    }
    var it = session.items[i];
    var pattern = (it.breathPattern && it.breathPattern.length)
      ? it.breathPattern.map(function (p) { return p.charAt(0).toUpperCase(); }).join('')
      : 'IE';
    var mode = it.mode === 'hundred' ? 1 : it.mode === 'continuous' ? 2 : 0;
    Pebble.sendAppMessage({
      CHUNK_INDEX: i,
      ITEM_NAME: it.name,
      ITEM_REPS: it.reps || 0,
      ITEM_LENGTH_DS: Math.round((it.movementLengthSec || 0) * 10),
      ITEM_REST: it.restAfterSec || 0,
      ITEM_BETWEEN_DS: Math.round((it.betweenRepsSec == null ? 1 : it.betweenRepsSec) * 10),
      ITEM_MODE: mode,
      ITEM_PATTERN: pattern
    }, function () { sendItem(i + 1); },
       function (e) { console.log('Pilatable: item ' + i + ' send failed: ' + JSON.stringify(e)); });
  }

  Pebble.sendAppMessage(header, function () { sendItem(0); },
    function (e) { console.log('Pilatable: header send failed: ' + JSON.stringify(e)); });
}

Pebble.addEventListener('ready', function () {
  console.log('Pilatable JS ready');
});

// Watch asks for the latest session on launch.
Pebble.addEventListener('appmessage', function (e) {
  if (e.payload && e.payload.REQUEST_SESSION) {
    console.log('Pilatable: watch requested session');
    sendSession(loadStored());
  }
});

// Open the hosted config page, seeding it with the current session.
Pebble.addEventListener('showConfiguration', function () {
  var current = encodeURIComponent(JSON.stringify(loadStored()));
  Pebble.openURL(CONFIG_URL + '?session=' + current);
});

// Persist what the config page returns, then push it to the watch if present.
Pebble.addEventListener('webviewclosed', function (e) {
  if (!e.response) return;
  try {
    var session = JSON.parse(decodeURIComponent(e.response));
    localStorage.setItem(STORAGE_KEY, JSON.stringify(session));
    console.log('Pilatable: saved session "' + session.name + '"');
    sendSession(session);
  } catch (err) {
    console.log('Pilatable: bad config response: ' + err);
  }
});
