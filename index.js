const {
  CronJob
} = require('cron');
const got = require('got');
const twilio = require('twilio');

// Needed for the Twitch API
const clientId = process.env.TWITCH_CLIENT_ID;
const channelName = process.env.TWITCH_CHANNEL_NAME;

// We are using a Twilio Sync document to interact with the ESP8266
const syncService = twilio()
  .sync.services(process.env.TWILIO_SYNC_SERVICE)
const syncDocument = syncService
  .documents(process.env.TWILIO_DOCUMENT_NAME);

if (!clientId || !channelName) {
  console.error('Missing Twitch client ID or channel name');
  process.exit(1);
}

const twitchUrl = `https://api.twitch.tv/kraken/streams/${channelName}?client_id=${clientId}`;

let isOnline = false;

async function checkStreamStatus() {
  const resp = await got(twitchUrl, {
    json: true
  });

  const {
    stream
  } = resp.body;
  const isCurrentlyOnline = stream !== null;

  if (isOnline !== isCurrentlyOnline) {
    isOnline = isCurrentlyOnline;
    const led = isOnline ? 'ON' : 'OFF';
    console.log(`Setting LED to ${led}`);
    const doc = await syncDocument.update({
      data: {
        led
      }
    });
  }
}

async function start() {
  // Create the Sync document in case it doesn't exist
  try {
    await syncService.documents.create({
      uniqueName: process.env.TWILIO_DOCUMENT_NAME,
      data: {
        led: 'OFF'
      }
    })
  } catch (err) {
    console.log('Document already exists.');
  }

  console.log('Starting cron job!');
  const job = new CronJob('*/20 * * * * *', checkStreamStatus);
  job.start();
}

start().then(() => {
  console.log('Listening for updates...');
}).catch(err => {
  console.error(err);
});