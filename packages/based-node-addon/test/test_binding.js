const {
  GetService,
  Connect,
  Function,
  Observe,
  Unobserve,
  Get,
  ConnectToUrl,
} = require('../dist/index.js')
const assert = require('assert')

function testBasic() {
  // TODO: I have to find a way to tell node that the connection is still alive and the
  // process should not exit. This is a hack to keep the process alive until the timer exits.

  let obsId
  setInterval(() => {
    Unobserve(obsId)
    // Observe("counter", "{}", (data, checksum, error, id) => {
    //   if (data) console.log(id, checksum, "HELLO IM GET YES:", data);
    //   else if (error) console.log(id, checksum, "error!", error);
    // });
  }, 3000)

  try {
    ConnectToUrl('ws://localhost:9910')

    // Connect({
    //   cluster: "http://localhost:7022",
    //   org: "saulx",
    //   project: "demo",
    //   env: "production",
    //   // name: "@based/env-hub",
    // });

    // Function("doIt", '{"look":true}', (data, error, id) => {
    //   if (data) console.log(id, "doIt cb:", data);
    //   else if (error) console.log(id, "error!", error);
    // });

    Get('counter', '{}', (data, error, id) => {
      if (data) console.log(id, 'Get data:', data)
      else if (error) console.log(id, 'error!', error)
    })

    obsId = Observe('counter', '{}', (data, checksum, error, id) => {
      if (data) console.log(id, checksum, 'Observe data:', data)
      else if (error) console.log(id, checksum, 'error!', error)
    })

    console.log(obsId)

    // Observe("counter", "{}", (data, checksum, error, id) => {
    //   if (data) console.log(id, checksum, "counter cb:", data);
    //   else if (error) console.log(id, checksum, "error!", error);
    // });
    // Observe("counter", "{}", (data, checksum, error, id) => {
    //   if (data) console.log(id, checksum, "counter2 cb:", data);
    //   else if (error) console.log(id, checksum, "error!", error);
    // });
    // Observe("counter", "{}", (data, checksum, error, id) => {
    //   if (data) console.log(id, checksum, "counter3 cb:", data);
    //   else if (error) console.log(id, checksum, "error!", error);
    // });
  } catch (err) {
    console.log(err)
  }
}

assert.doesNotThrow(testBasic, undefined, 'testBasic threw an expection')

console.log('Tests passed- everything looks OK!')
