import test from 'ava'
import { BasedClient } from '../lib'
import { BasedServer } from '@based/server'
import { wait } from '@saulx/utils'

const setup = async () => {
  const client = new BasedClient()
  const server = new BasedServer({
    port: 9910,
    rateLimit: { http: 1e6, ws: 1e6, drain: 1 },
    functions: {
      configs: {
        counter: {
          type: 'query',
          fn: async (_, __, update) => {
            let cnt = 0
            update(cnt)
            const counter = setInterval(() => {
              update(++cnt)
            }, 300)
            return () => {
              clearInterval(counter)
            }
          },
        },
        text: {
          type: 'function',
          uninstallAfterIdleTime: 1e3,
          fn: async (based, payload, ctx) => {
            return 'here is a nice piece of text' + String(Date.now())
          },
        },
      },
    },
  })
  await server.start()
  await wait(500)
  return { client, server }
}

test.serial('10k concurrent calls', async (t) => {
  const { client, server } = await setup()

  t.teardown(() => {
    client.disconnect()
    server.destroy()
  })

  client.connect({
    url: async () => {
      return 'ws://localhost:9910'
    },
  })

  await wait(500)

  const q = Array(10_000)
    .fill(undefined)
    .map(() => {
      return client.call('text')
    })

  await t.notThrowsAsync(Promise.all(q))
})

test.serial('10k concurrent gets', async (t) => {
  const { client, server } = await setup()

  t.teardown(() => {
    client.disconnect()
    server.destroy()
  })

  client.connect({
    url: async () => {
      return 'ws://localhost:9910'
    },
  })

  await wait(500)

  const q = Array(10_000)
    .fill(undefined)
    .map(() => {
      return client.query('counter').get()
    })

  await t.notThrowsAsync(Promise.all(q))
})

test.serial('10k concurrent subs', async (t) => {
  const { client, server } = await setup()

  t.teardown(() => {
    client.disconnect()
    server.destroy()
  })

  client.connect({
    url: async () => {
      return 'ws://localhost:9910'
    },
  })

  await wait(500)

  const size = 10_000
  let count = 0

  Array(size)
    .fill(undefined)
    .map(() => {
      return client.query('counter').subscribe(() => {
        count++
      })
    })

  await wait(600)

  t.assert(count >= size)
})
