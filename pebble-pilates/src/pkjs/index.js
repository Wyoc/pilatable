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

var STORAGE_KEY = 'pilatable.collection.v1';
var LEGACY_KEY = 'pilatable.session.v1';

// A collection holds several named sessions; one is active (synced to the watch).
// Settings are global. The config page is the source of truth; pkjs persists what
// it returns and relays the active session to the watch.

function defaultSession() {
  var IE = ['inhale', 'exhale'], EI = ['exhale', 'inhale'], EIE = ['exhale', 'inhale', 'exhale'];
  return {
    id: 'fundamental', name: 'Fundamental Mat',
    items: [
      { id: 'pelvic-curl', name: 'Pelvic Curl', reps: 5, movementLengthSec: 5.0, restAfterSec: 30, breathPattern: EIE },
      { id: 'chest-lift', name: 'Chest Lift', reps: 10, movementLengthSec: 4.0, restAfterSec: 30, breathPattern: EIE },
      { id: 'leg-lift-supine', name: 'Leg Lift Supine', reps: 5, movementLengthSec: 4.0, restAfterSec: 30, breathPattern: EI },
      { id: 'spine-twist-supine', name: 'Spine Twist Supine', reps: 5, movementLengthSec: 4.0, restAfterSec: 30, breathPattern: EI },
      { id: 'chest-lift-with-rotation', name: 'Chest Lift With Rotation', reps: 5, movementLengthSec: 4.0, restAfterSec: 30, breathPattern: EI },
      { id: 'back-extension-prone', name: 'Back Extension Prone', reps: 5, movementLengthSec: 5.0, restAfterSec: 30, breathPattern: EI },
      { id: 'one-leg-circle', name: 'One-Leg Circle', reps: 5, movementLengthSec: 4.0, restAfterSec: 30, breathPattern: EI },
      { id: 'rolling-back', name: 'Rolling Back', reps: 10, movementLengthSec: 4.0, restAfterSec: 0, breathPattern: IE }
    ]
  };
}

function defaultCollection() {
  var s = defaultSession();
  return { version: 3, activeId: s.id,
    settings: { hapticsEnabled: true, intensity: 1, leadInEnabled: true }, sessions: [s] };
}

function loadCollection() {
  try {
    var raw = localStorage.getItem(STORAGE_KEY);
    if (raw) { var c = JSON.parse(raw); if (c && c.sessions && c.sessions.length) return c; }
    var legacy = localStorage.getItem(LEGACY_KEY);
    if (legacy) {  // migrate the old single-session format
      var s = JSON.parse(legacy);
      return { version: 3, activeId: 'migrated',
        settings: s.settings || { hapticsEnabled: true, intensity: 1, leadInEnabled: true },
        sessions: [{ id: 'migrated', name: s.name || 'My Mat Session', items: s.items || [] }] };
    }
  } catch (e) { console.log('Pilatable: failed to load collection: ' + e); }
  return defaultCollection();
}

function saveCollection(c) {
  try { localStorage.setItem(STORAGE_KEY, JSON.stringify(c)); }
  catch (e) { console.log('Pilatable: save failed: ' + e); }
}

// The single session the watch runs = the active one + global settings.
function activeSession(c) {
  var s = c.sessions[0];
  for (var i = 0; i < c.sessions.length; i++) if (c.sessions[i].id === c.activeId) s = c.sessions[i];
  return { version: 2, name: s ? s.name : 'Session',
    settings: c.settings || {}, items: s ? s.items : [] };
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

// Watch asks for the latest session on launch -> send the active one.
Pebble.addEventListener('appmessage', function (e) {
  if (e.payload && e.payload.REQUEST_SESSION) {
    console.log('Pilatable: watch requested session');
    sendSession(activeSession(loadCollection()));
  }
});

// Open the hosted config page, seeding it with the whole session collection.
Pebble.addEventListener('showConfiguration', function () {
  var data = encodeURIComponent(JSON.stringify(loadCollection()));
  Pebble.openURL(CONFIG_URL + '?data=' + data);
});

// Persist the returned collection, then push the active session to the watch.
Pebble.addEventListener('webviewclosed', function (e) {
  if (!e.response) return;
  try {
    var collection = JSON.parse(decodeURIComponent(e.response));
    if (!collection || !collection.sessions) return;
    saveCollection(collection);
    var s = activeSession(collection);
    console.log('Pilatable: saved ' + collection.sessions.length + ' session(s), active "' + s.name + '"');
    sendSession(s);
  } catch (err) {
    console.log('Pilatable: bad config response: ' + err);
  }
});
