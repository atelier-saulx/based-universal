const {
  GetService,
  Connect,
  Function,
  Observe,
  Unobserve,
  Get,
  ConnectToUrl,
} = require("../dist/binding.js");
const assert = require("assert");

function testBasic() {
  setTimeout(() => {}, 10e3);
  try {
    ConnectToUrl("ws://localhost:9910");

    // Connect({
    //   cluster: "http://localhost:7022",
    //   org: "saulx",
    //   project: "demo",
    //   env: "production",
    //   // name: "@based/edge",
    // });

    // const x = GetService({
    //   cluster: "https://d15p61sp2f2oaj.cloudfront.net",
    //   org: "saulx",
    //   project: "demo",
    //   env: "production",
    //   name: "@based/edge",
    // });
    // console.log(x);

    // Function("doIt", '{"look":true}', (data, error, id) => {
    //   if (data) console.log(id, "doIt cb:", data);
    //   else if (error) console.log(id, "error!", error);
    // });

    Observe("counter", "{}", (data, checksum, error, id) => {
      if (data) console.log(id, checksum, "counter cb:", data);
      else if (error) console.log(id, checksum, "error!", error);
    });
  } catch (err) {
    console.log(err);
  }
}

assert.doesNotThrow(testBasic, undefined, "testBasic threw an expection");

console.log("Tests passed- everything looks OK!");
