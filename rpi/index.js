import hachiNIO from "hachi-nio";
import express from "express";
import http from "http";
import * as socketIO from "socket.io";

let server = new hachiNIO.server(7890);
let devices = {};

const app = express();
const httpServer = http.Server(app);
const io = new socketIO.Server(httpServer);

let history = {};

io.on('connection', function(socket) {

    socket.on('LUX', (data) => {
        if(devices[data.device]){
            history[data.device] = Math.round(parseFloat(data.value) * 100);
            console.log('sending to ' + data.device + ", value: " + history[data.device]);
            hachiNIO.send(devices[data.device], {transaction : "LUX"}, history[data.device].toString());
        }
    });

});


app.use(express.static('public'));

httpServer.listen(3000, function() {
    console.log('listening on *:3000');
});

////////

server.on('server_listening', () => {
    console.log("Server is up! Now waiting for connections");
});

server.on('client_connected', (socketClient) => {
    console.log("NEW CLIENT CONNECTED! \t id:"+socketClient.id+" origin:"+socketClient.address().address);

    //hachiNIO.send(socketClient, {transaction : "HB"}, "HB");
    //hachiNIO.send(socketClient, {transaction : "HB"}, "HB");
});

server.on('client_close', (socketClient) => {
    console.log("CLIENT DISCONNECTED! \t id:"+socketClient.id);
});

server.on("data", (socketClient, header, dataBuffer) => {

    //console.log(header);

    switch(header.transaction){
        case "LOGIN":

            let deviceId = dataBuffer.toString();

            if(devices[deviceId]){
                devices[deviceId].end();
            }

            devices[deviceId] = socketClient;
            devices[deviceId].lastHB = Date.now();
            devices[deviceId].deviceLabel = deviceId;
            console.log("NEW DEVICE:" + deviceId);

            if(history[deviceId]){
                hachiNIO.send(devices[deviceId], {transaction : "LUX"}, history[deviceId].toString());
            }

            break;
        case "HB":

            if(!devices[dataBuffer.toString()]){
                console.log("HB for invalid DEVICE:" + dataBuffer.toString());
                return;
            }

            console.log("HB for DEVICE:" +  dataBuffer.toString());
            devices[dataBuffer.toString()].lastHB = Date.now();
            break;
    }

    //console.log("MESSAGE RECEIVED! \t id:"+socketClient.id+" message:"+dataBuffer.toString());
});

setInterval(() => {

    let deadList = [];

    Object.keys(devices).forEach((label) => {

        let d = devices[label];

        console.log(d.lastHB  + (30 * 1000) < Date.now())
        if(d.lastHB  + (30 * 1000) < Date.now()){
            console.log("TIMEOUT FOR:" + label);
            d.end();
            deadList.push(label);
        }
    });

    deadList.forEach((l)=>{
        console.log("REMOVEING:" + l);
        delete devices[l];
    });

}, 5000);