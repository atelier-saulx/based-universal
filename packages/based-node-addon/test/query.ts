import { BasedServer } from '@based/server'
import { BasedClient } from '../dist'
import { wait } from '@saulx/utils'
import test from 'ava'

test.serial('query functions', async (t) => {
  const client = new BasedClient()
  const server = new BasedServer({
    port: 9910,
    functions: {
      configs: {
        counter: {
          type: 'query',
          uninstallAfterIdleTime: 1e3,
          fn: (_, __, update) => {
            let cnt = 0
            update(cnt)
            const counter = setInterval(() => {
              update(++cnt)
            }, 1000)
            return () => {
              clearInterval(counter)
            }
          },
        },
      },
    },
  })
  await server.start()

  client.connect({
    url: async () => {
      return 'ws://localhost:9910'
    },
  })

  const obs1Results: any[] = []
  const obs2Results: any[] = []

  const close = client
    .query('counter', {
      myQuery: 123,
    })
    .subscribe((d) => {
      obs1Results.push(d)
    })

  const close2 = client
    .query('counter', {
      myQuery: 123,
    })
    .subscribe((d) => {
      obs2Results.push(d)
    })

  await wait(500)
  close()
  server.functions.updateInternal({
    type: 'query',
    name: 'counter',
    version: 2,
    uninstallAfterIdleTime: 1e3,
    maxPayloadSize: 1e9,
    rateLimitTokens: 1,
    fn: (_, __, update) => {
      let cnt = 0
      const counter = setInterval(() => {
        update('counter2:' + ++cnt)
      }, 100)
      return () => {
        clearInterval(counter)
      }
    },
  })
  await wait(1e3)
  close2()
  t.true(obs1Results.length < obs2Results.length)
  t.true(obs2Results[obs2Results.length - 1].startsWith('counter2:'))
  await wait(100)
  t.is(Object.keys(server.activeObservables).length, 1)
  t.is(server.activeObservablesById.size, 1)
  await wait(5000)
  t.is(Object.keys(server.activeObservables).length, 0)
  t.is(server.activeObservablesById.size, 0)
  await wait(6e3)
  t.is(Object.keys(server.functions.specs).length, 0)
})

// function testBasic() {
//   // TODO: I have to find a way to tell node that the connection is still alive and the
//   // process should not exit. This is a hack to keep the process alive until the timer exits.

//   let obsId
//   setInterval(() => {
//     Unobserve(obsId)
//     // Observe("counter", "{}", (data, checksum, error, id) => {
//     //   if (data) console.log(id, checksum, "HELLO IM GET YES:", data);
//     //   else if (error) console.log(id, checksum, "error!", error);
//     // });
//   }, 3000)

//   try {
//     ConnectToUrl('ws://localhost:9910')

//     // Connect({
//     //   cluster: "http://localhost:7022",
//     //   org: "saulx",
//     //   project: "demo",
//     //   env: "production",
//     //   // name: "@based/env-hub",
//     // });

//     // Function("doIt", '{"look":true}', (data, error, id) => {
//     //   if (data) console.log(id, "doIt cb:", data);
//     //   else if (error) console.log(id, "error!", error);
//     // });

//     Get('counter', '{}', (data, error, id) => {
//       if (data) console.log(id, 'Get data:', data)
//       else if (error) console.log(id, 'error!', error)
//     })

//     obsId = Observe('counter', '{}', (data, checksum, error, id) => {
//       if (data) console.log(id, checksum, 'Observe data:', data)
//       else if (error) console.log(id, checksum, 'error!', error)
//     })

//     console.log(obsId)

//     // Observe("counter", "{}", (data, checksum, error, id) => {
//     //   if (data) console.log(id, checksum, "counter cb:", data);
//     //   else if (error) console.log(id, checksum, "error!", error);
//     // });
//     // Observe("counter", "{}", (data, checksum, error, id) => {
//     //   if (data) console.log(id, checksum, "counter2 cb:", data);
//     //   else if (error) console.log(id, checksum, "error!", error);
//     // });
//     // Observe("counter", "{}", (data, checksum, error, id) => {
//     //   if (data) console.log(id, checksum, "counter3 cb:", data);
//     //   else if (error) console.log(id, checksum, "error!", error);
//     // });
//   } catch (err) {
//     console.log(err)
//   }
// }

// assert.doesNotThrow(testBasic, undefined, 'testBasic threw an expection')

// console.log('Tests passed- everything looks OK!')
