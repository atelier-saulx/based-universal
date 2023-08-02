/**
 * To keep the event loop from exiting, might have to do some hacky stuff:
 * https://stackoverflow.com/questions/39082527/how-to-prevent-the-nodejs-event-loop-from-exiting
 */

var BasedClient = require('./dist/index.js')
var process = require('process')

var stop = false
var f = function () {
  if (!stop) process.nextTick(f)
}
f()

console.log(BasedClient) // 'world'
