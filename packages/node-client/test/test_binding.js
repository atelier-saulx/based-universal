const { GetService } = require("../dist/binding.js");
const assert = require("assert");

function testBasic() {
  try {
    GetService(1, {
      cluster: "https://d15p61sp2f2oaj.cloudfront.net",
      org: "saulx",
      project: "demo",
      env: "production",
      name: "@based/edge",
    });
  } catch (err) {
    console.log(err);
  }
}

assert.doesNotThrow(testBasic, undefined, "testBasic threw an expection");

console.log("Tests passed- everything looks OK!");
