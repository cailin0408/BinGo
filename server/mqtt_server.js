const mqtt = require("mqtt");

// 連線到 Raspberry Pi 本機的 Mosquitto Broker
const client = mqtt.connect("mqtt://localhost");

// 必須跟 handTracking.html 使用相同的 Topic
const TOPIC = "bingo/command";

// 成功連上 MQTT Broker
client.on("connect", () => {
    console.log("=================================");
    console.log("MQTT connected");
    console.log("=================================");

    // 訂閱手勢控制 Topic
    client.subscribe(TOPIC, (err) => {

        if (err) {
            console.error("Subscribe failed:", err);
            return;
        }

        console.log(`Subscribed: ${TOPIC}`);
        console.log("Waiting for gesture command...");
    });
});

// 收到 MQTT 訊息
client.on("message", (topic, message) => {

    // MQTT 收到的是 Buffer，轉成字串
    const data = message.toString();

    console.log("\n-----------------------------");
    console.log(`Topic   : ${topic}`);
    console.log(`Message : ${data}`);

    // 根據手勢指令執行不同動作
    switch (data) {

        case "COME":
            console.log(">>> COME：垃圾桶開始跟隨");
            break;

        case "STOP":
            console.log(">>> STOP：垃圾桶停止");
            break;

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