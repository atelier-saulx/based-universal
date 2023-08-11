import test, { ExecutionContext } from 'ava'
import { BasedClient } from '..'
import { BasedServer } from '@based/server'
import { BasedError, BasedErrorCode } from '../lib/types/error'
import { BasedQueryFunction, ObservableUpdateFunction } from '@based/functions'
import { wait } from '@saulx/utils'

const throwingFunction = async () => {
  throw new Error('This is error message')
}

const counter: BasedQueryFunction = (_, __, update) => {
  update({ yeye: 'yeye' })
  throw new Error('bla')
  // return () => undefined
}

const errorFunction = async () => {
  const wawa = [1, 2]
  // @ts-ignore
  return wawa[3].yeye
}

const errorTimer = (_: any, __: any, update: ObservableUpdateFunction) => {
  const int = setInterval(() => {
    update(undefined, undefined, new Error('lol'))
  }, 10)
  update('yes')
  return () => {
    clearInterval(int)
  }
}

const alternatingError = (
  _: any,
  __: any,
  update: ObservableUpdateFunction
) => {
  let cnt = 0
  const int = setInterval(() => {
    if (++cnt % 2) {
      update(undefined, undefined, new Error('derp'))
    } else {
      update('hello')
    }
  }, 20)
  update('doei')
  return () => {
    clearInterval(int)
  }
}

const setup = async (t: ExecutionContext) => {
  // t.timeout(4000)
  const coreClient = new BasedClient()

  const server = new BasedServer({
    port: 9910,
    functions: {
      configs: {
        throwingFunction: {
          type: 'function',
          uninstallAfterIdleTime: 1e3,
          fn: throwingFunction,
        },
        errorFunction: {
          type: 'function',
          uninstallAfterIdleTime: 1e3,
          fn: errorFunction,
        },
        counter: {
          type: 'query',
          uninstallAfterIdleTime: 1e3,
          fn: counter,
        },
        errorTimer: {
          type: 'query',
          uninstallAfterIdleTime: 1,
          fn: errorTimer,
        },
        acdc: {
          type: 'query',
          uninstallAfterIdleTime: 1,
          fn: alternatingError,
        },
      },
    },
  })
  await server.start()

  t.teardown(() => {
    coreClient.disconnect()
    server.destroy()
  })

  return { coreClient, server }
}

test.serial('function error', async (t) => {
  const { coreClient } = await setup(t)

  coreClient.connect({
    url: async () => {
      return 'ws://localhost:9910'
    },
  })

  // TODO: Check error instance of
  const error = (await t.throwsAsync(
    coreClient.call('throwingFunction')
  )) as BasedError

  t.is(error.code, BasedErrorCode.FunctionError)
})

test.serial('function authorize error', async (t) => {
  const { coreClient, server } = await setup(t)

  server.auth.updateConfig({
    authorize: throwingFunction,
  })

  coreClient.connect({
    url: async () => {
      return 'ws://localhost:9910'
    },
  })

  // TODO: Check error instance of
  const error = (await t.throwsAsync(
    coreClient.call('throwingFunction')
  )) as BasedError
  t.is(error.code, BasedErrorCode.AuthorizeFunctionError)
})

test.serial('observable authorize error', async (t) => {
  const { coreClient, server } = await setup(t)

  server.auth.updateConfig({
    authorize: throwingFunction,
  })

  coreClient.connect({
    url: async () => {
      return 'ws://localhost:9910'
    },
  })

  // TODO: Check error instance of
  const error = (await new Promise((resolve) => {
    coreClient.query('counter', {}).subscribe(
      (v) => {
        console.info({ v })
      },

      (err) => {
        resolve(err)
      }
    )
  })) as BasedError
  t.is(error.code, BasedErrorCode.AuthorizeFunctionError)
})

test.serial('type error in function', async (t) => {
  const { coreClient } = await setup(t)

  coreClient.connect({
    url: async () => {
      return 'ws://localhost:9910'
    },
  })

  // TODO: Check error instance of
  const error = (await t.throwsAsync(
    coreClient.call('errorFunction')
  )) as BasedError
  t.is(error.code, BasedErrorCode.FunctionError)
})

// TODO: Will be handled by transpilation of the function (wrapping set inerval / timeout)
test.serial('throw in an interval', async (t) => {
  const { coreClient } = await setup(t)
  coreClient.connect({
    url: async () => {
      return 'ws://localhost:9910'
    },
  })
  await t.throwsAsync(
    new Promise((_, reject) =>
      coreClient.query('errorTimer', {}).subscribe(() => {}, reject)
    )
  )
})

test.only('unobserve errored query twice', async (t) => {
  const { coreClient } = await setup(t)
  coreClient.connect({
    url: async () => {
      return 'ws://localhost:9910'
    },
  })

  let dataCnt = 0
  let errCnt = 0

  const one = coreClient.query('acdc', {}).subscribe(
    () => {
      dataCnt++
    },
    () => {
      errCnt++
    }
  )
  await wait(300)

  one()
  t.assert(dataCnt > 2)
  t.assert(errCnt > 2)

  dataCnt = 0
  errCnt = 0

  const two = coreClient.query('acdc', {}).subscribe(
    () => {
      dataCnt++
    },
    () => {
      errCnt++
    }
  )
  await wait(300)
  two()
  t.assert(dataCnt > 2)
  t.assert(errCnt > 2)
})
