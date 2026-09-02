const mqtt = require("mqtt");
const fs = require("fs");
const { spawn } = require("child_process");

// 連線到 Raspberry Pi 本機的 Mosquitto Broker
const client = mqtt.connect("mqtt://localhost");

// 必須跟 handTracking.html 使用相同的 Topic
const TOPIC = "bingo/command";

// Buzzer Device Driver
const BUZZER_DEVICE = "/dev/buzzer";

// 垃圾車音樂
const MUSIC_FILE = "/home/pi/garbage/MQTT-Server/garbage.mp3";

// 儲存目前的音樂 Process
let musicProcess = null;


// ===============================
// 蜂鳴器
// ===============================
function beep(duration = 300) {
    try {
        fs.writeFileSync(BUZZER_DEVICE, "1");

        setTimeout(() => {
            try {
                fs.writeFileSync(BUZZER_DEVICE, "0");
            } catch (err) {
                console.error("Buzzer OFF failed:", err.message);
            }
        }, duration);

    } catch (err) {
        console.error("Buzzer ON failed:", err.message);
    }
}


// ===============================
// 播放垃圾車音樂
// ===============================
function playMusic() {

    // 如果已經在播放，就不要再開新的 mpg123
    if (musicProcess) {
        console.log(">>> 音樂已經在播放，不重複啟動");
        return;
    }

    console.log(">>> 開始播放垃圾車音樂");

    musicProcess = spawn("mpg123", [MUSIC_FILE]);

    // 音樂正常播放結束
    musicProcess.on("close", () => {
        console.log(">>> 垃圾車音樂播放結束");
        musicProcess = null;
    });

    // 播放發生錯誤
    musicProcess.on("error", (err) => {
        console.error("播放音樂失敗:", err.message);
        musicProcess = null;
    });
}


// ===============================
// 停止垃圾車音樂
// ===============================
function stopMusic() {

    if (musicProcess) {
        console.log(">>> 停止垃圾車音樂");

        musicProcess.kill();
        musicProcess = null;
    }
}


// ===============================
// MQTT 連線成功
// ===============================
client.on("connect", () => {

    console.log("=================================");
    console.log("MQTT connected");
    console.log("=================================");

    client.subscribe(TOPIC, (err) => {

        if (err) {
            console.error("Subscribe failed:", err);
            return;
        }

        console.log(`Subscribed: ${TOPIC}`);
        console.log("Waiting for gesture command...");
    });
});


// ===============================
// 收到 MQTT 訊息
// ===============================
client.on("message", (topic, message) => {

    const data = message.toString();

    console.log("\n-----------------------------");
    console.log(`Topic   : ${topic}`);
    console.log(`Message : ${data}`);

    switch (data) {

        // ✋ 布
        case "COME":

            console.log(">>> COME：垃圾桶開始跟隨");
            console.log(">>> MUSIC：播放垃圾車音樂");

            playMusic();

            break;


        // ✊ 石頭
        case "STOP":

            console.log(">>> STOP：垃圾桶停止");

            // 停止垃圾車音樂
            stopMusic();

            // 蜂鳴器嗶一聲
            console.log(">>> Buzzer：BEEP");
            beep(300);

            break;


        // 👍 讚
        case "OPEN_LID":

            console.log(">>> OPEN_LID：開啟垃圾桶蓋");

            break;


        default:

            console.log(`>>> Unknown command: ${data}`);

            break;
    }

    console.log("-----------------------------");
});


// MQTT 發生錯誤
client.on("error", (err) => {
    console.error("MQTT Error:", err.message);
});


// MQTT 連線中斷
client.on("close", () => {
    console.log("MQTT connection closed");
});