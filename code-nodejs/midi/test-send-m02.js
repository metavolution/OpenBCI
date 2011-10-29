

var MIDI = require('MIDI');

var midi01 = MIDI.outputPorts();
console.log( "midi devices:");
console.log( midi01);
console.log( "--------");

//var output = new MIDI.MIDIOutput(undefined, 1);

var output = new MIDI.MIDIOutput("TASCAM US-122L MIDI 1", 1);
//var output = new MIDI.MIDIOutput("Midi Through Port-0", 1);

console.log('opened MIDI output port', output.portName);

output.send('f0 00 01 02 f7');



function sendSomeNotes(channel, basePitch, velocity) {
    output.channel(channel);
    for (var i = 0; i < 100; i++) {
        var pitch = basePitch + i * 12;
        var period = 400;

        output.noteOn(pitch, velocity, i * period);
        output.noteOn(pitch+3, velocity, i * period);
        output.noteOn(pitch+7, velocity, i * period);
        output.noteOn(pitch+10, velocity, i * period);

        output.noteOn(pitch, 0, i * period + 350);
        output.noteOn(pitch+3, 0, i * period + 350);
        output.noteOn(pitch+7, 0, i * period + 350);
        output.noteOn(pitch+10, 0, i * period + 350);

        console.log('sent ' + i);
    }
}


function sendSomeNotes2(channel, basePitch, velocity) {
    output.channel(channel);
    for (var i = 0; i < 10; i++) {
        var pitch = basePitch + i * 12;
        var period = 400;

        output.noteOn(pitch, velocity, i * period);
        output.noteOn(pitch+3, velocity, i * period);
        output.noteOn(pitch+7, velocity, i * period);
        output.noteOn(pitch+10, velocity, i * period);

        output.noteOn(pitch, 0, i * period + 350);
        output.noteOn(pitch+3, 0, i * period + 350);
        output.noteOn(pitch+7, 0, i * period + 350);
        output.noteOn(pitch+10, 0, i * period + 350);

        console.log('sent ' + i);
    }
}

function sendSomeNotes2(channel, basePitch, velocity) {
    output.channel(channel);
    for (var i = 0; i < 10; i++) {
        var pitch = basePitch + i * 12;
        var period = 400;

        output.noteOn(pitch, velocity, i * period);
        output.noteOn(pitch+3, velocity, i * period);
        output.noteOn(pitch+7, velocity, i * period);
        output.noteOn(pitch+10, velocity, i * period);

        output.noteOn(pitch, 0, i * period + 350);
        output.noteOn(pitch+3, 0, i * period + 350);
        output.noteOn(pitch+7, 0, i * period + 350);
        output.noteOn(pitch+10, 0, i * period + 350);

        console.log('sent ' + i);
    }
}


sendSomeNotes(1, 32, 30);

sendSomeNotes(2, 64, 30);

sendSomeNotes(4, 64, 127);

sendSomeNotes3(2, 32, 127);

//sendSomeNotes2(1, 64, 45);

output.controlChange(20, 30);
output.channel(2);
output.controlChange(20, 30);

output.nrpn14(1024, 1024);
output.nrpn7(1024, 127);
try {
    output.nrpn7(1024, 1024);
}
catch (e) {
    console.log('caught expected error:', e);
}

/*

console.log('starting timer', MIDI.currentTime());
MIDI.at(3000, function () { console.log("should be all done, time is", MIDI.currentTime()); });
console.log('done', MIDI.currentTime());

process.on('exit', function () { console.log('exiting, current time is', MIDI.currentTime()); });

*/
