import Emitter from './Emitter'
import { BasedQuery } from './Query'
import { AuthState, BasedOpts, Settings } from './types'

const {
  NewClient,
  Connect,
  ConnectToUrl,
  Disconnect,
  Call,
  SetAuthstate,
} = require('../build/Release/based-node-addon')

// export = {
//   GetService: addon.GetService,
//   Connect: addon.Connect,
//   Function: addon.Function,
//   ConnectToUrl: addon.ConnectToUrl,
//   Observe: addon.Observe,
//   Unobserve: addon.Unobserve,
//   Get: addon.Get,
// };

export class BasedClient extends Emitter {
  constructor(opts?: BasedOpts) {
    super()

    this.clientId = NewClient()

    if (opts && Object.keys(opts).length > 0) {
      this.opts = opts
      this.connect(opts)
    }
  }

  clientId: number

  // --------- Connection State
  // @ts-ignore TODO: why
  opts: BasedOpts
  // connected: boolean = false // TODO: add connected method in c++

  // ---------- Destroy
  public isDestroyed?: boolean

  // --------- Queue
  // maxPublishQueue: number = 1000
  // publishQueue: ChannelPublishQueue = []
  // functionQueue: FunctionQueue = []
  // observeQueue: ObserveQueue = new Map()
  // channelQueue: ChannelQueue = new Map()
  // getObserveQueue: GetObserveQueue = new Map()

  // --------- Channel State
  // channelState: ChannelState = new Map()
  // channelCleanTimeout?: ReturnType<typeof setTimeout>
  // channelCleanupCycle: number = 30e3
  // // --------- Observe State
  // observeState: ObserveState = new Map()
  // // --------- Get State
  // getState: GetState = new Map()

  // -------- Auth state
  // authState: AuthState = {} // TODO: add method to check authState in c++

  // --------- Internal Events

  // --------- Connect
  public async connect(opts?: BasedOpts) {
    if (opts) {
      if (this.opts) {
        this.disconnect()
      }
      this.opts = opts
    }
    if (!this.opts) {
      console.error('Configure opts to connect')
      return
    }

    opts ??= this.opts

    if (opts?.url) {
      let url: string = ''
      if (typeof opts.url === 'string') {
        url = opts.url
      } else if (
        opts.url instanceof Promise ||
        typeof opts.url === 'function'
      ) {
        url = await opts.url()
      } else {
        throw new Error(`opts.url is wrong type: ${typeof opts.url}`)
      }
      ConnectToUrl(this.clientId, url)
    } else {
      const { cluster, org, project, env, name, key, optionalKey } = opts
      Connect(this.clientId, cluster, org, project, env, name, key, optionalKey)
    }
  }

  public disconnect() {
    Disconnect(this.clientId)
  }

  public async destroy() {
    this.disconnect()
    for (const i in this) {
      delete this[i]
    }
    this.isDestroyed = true
  }

  // ---------- Channel

  // TODO:

  // channel(name: string, payload?: any): BasedChannel {
  //   // return new BasedChannel(this, name, payload)
  // }

  // ---------- Query

  query(name: string, payload?: any): BasedQuery {
    return new BasedQuery(this, name, payload)
  }

  // -------- Function
  call(name: string, payload?: any): Promise<any> {
    return new Promise((resolve, reject) => {
      Call(this.clientId, name, payload, (data, err, reqId) => {
        if (data) resolve(data)
        else if (err) reject(err)
      })
    })
  }

  // -------- Auth

  setAuthState(authState: AuthState): Promise<AuthState> {
    if (typeof authState === 'object') {
      return new Promise((resolve) => {
        SetAuthstate(this.clientId, authState, resolve)
      })
    } else {
      throw new Error('Invalid auth() arguments')
    }
  }

  clearAuthState(): Promise<AuthState> {
    return this.setAuthState({})
  }
}
