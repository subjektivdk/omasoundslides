.pragma library

// 83.5 → "1:23.50"
function time(seconds) {
    if (!isFinite(seconds) || seconds < 0)
        seconds = 0;
    var m = Math.floor(seconds / 60);
    var s = seconds - m * 60;
    var ss = s.toFixed(2);
    if (s < 10)
        ss = "0" + ss;
    return m + ":" + ss;
}

// 5 → "5.00"
function seconds(value) {
    return Number(value).toFixed(2);
}

// Accepts both "2.5" and "2,5". NaN when it isn't a number.
function parseSeconds(text) {
    return parseFloat(String(text).trim().replace(",", "."));
}

// Ruler labels: 65 → "1:05", 2.5 → "0:02.5"
function clock(seconds) {
    var m = Math.floor(seconds / 60);
    var s = seconds - m * 60;
    var whole = Math.floor(s);
    var text = m + ":" + (whole < 10 ? "0" : "") + whole;
    if (s - whole > 0.001)
        text += "." + Math.round((s - whole) * 10);
    return text;
}

var transitionLabels = {
    "none": "Cut",
    "fade": "Crossfade",
    "fadeblack": "Dip to black",
    "fadewhite": "Dip to white",
    "dissolve": "Dissolve"
};

function transition(name) {
    return transitionLabels[name] !== undefined ? transitionLabels[name] : name;
}

// Turns mouse wheel events into whole notches. Smooth-scrolling mice and
// touchpads send many small deltas; adding them up keeps every bit counted.
// `state` is any object that keeps the remainder between events.
function wheelNotches(state, event) {
    var delta = event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x;
    state.wheelRemainder = (state.wheelRemainder || 0) + delta;
    var notches = state.wheelRemainder > 0 ? Math.floor(state.wheelRemainder / 120)
                                           : Math.ceil(state.wheelRemainder / 120);
    state.wheelRemainder -= notches * 120;
    return notches;
}
