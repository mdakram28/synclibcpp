import WebSocket from 'ws';

const ws = new WebSocket('ws://localhost:8000', {
    perMessageDeflate: false
});

ws.on('error', console.error);

ws.on('open', function open() {
    console.log("Ws opened");
    // setTimeout(() => {
    //     console.log("Sending initial state");
    //     ws.send(JSON.stringify({
    //         diff: [],
    //         time: Date.now()
    //     }));

    //     setInterval(() => {
    //         console.log("Sending diff");
    //         ws.send(JSON.stringify({
    //             diff: {
    //                 "_t": 'A',
    //                 "0:0": [1]
    //             },
    //             time: Date.now()
    //         }));
    //     }, 4000);
        
    // }, 1000);
});

ws.on('message', function message(data: any) {
    console.log("received: %s", data);
});

setTimeout(() => {

}, 10000000000000);
