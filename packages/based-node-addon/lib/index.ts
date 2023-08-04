import { EventEmitter } from 'events'
import Emitter from './Emitter'
import { BasedQuery } from './Query'
import { AuthState, BasedOpts, Settings } from './types'
import { convertDataToBasedError } from './types/error'

const { NewClient, Connect, ConnectToUrl, Disconnect, Call, SetAuthState } =
  require('../build/Release/based-node-addon') as {
    NewClient: () => number
    Connect: (
      clientId: number,
      cluster: string,
      org: string,
      project: string,
      env: string,
      name?: string,
      key?: string,
      optionalKey?: boolean
    ) => void
    ConnectToUrl: (clientId: number, url: string) => void
    Disconnect: (clientId: number) => void
    Call: (
      clientId: number,
      name: string,
      payload: any,
      cb: (data: any, err: any, reqId: number) => void
    ) => void
    SetAuthState: (
      clientId: number,
      state: string,
      cb: (state: string) => void
    ) => void
  }

export class BasedClient extends Emitter {
  constructor(opts?: BasedOpts) {
    super()

    this.clientId = NewClient()

    // is this correct? or even necessary? check
    const refreshTimer = () => {
      this.keepAliveTimer = setTimeout(() => {
        refreshTimer()
      }, 2147483647)
    }

    this.keepAliveTimer = setTimeout(() => {
      refreshTimer()
    }, 2147483647)

    if (opts && Object.keys(opts).length > 0) {
      this.opts = opts
      this.connect(opts)
    }
  }

  clientId: number
  keepAliveTimer: NodeJS.Timer

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
      } else if (typeof opts.url === 'function') {
        url = await opts.url()
      } else {
        throw new Error(`opts.url is wrong type: ${typeof opts.url}`)
      }
      ConnectToUrl(this.clientId, url)
    } else {
      const { cluster, org, project, env, name, key, optionalKey } = opts
      if (!org || !project || !env) throw new Error('no org/project/env')

      Connect(
        this.clientId,
        cluster || 'production',
        org,
        project,
        env,
        name,
        key,
        optionalKey
      )
    }
  }

  public disconnect() {
    Disconnect(this.clientId)
    clearTimeout(this.keepAliveTimer)
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
      Call(this.clientId, name, JSON.stringify(payload), (data, err, reqId) => {
        if (data) resolve(JSON.parse(data))
        else if (err) reject(convertDataToBasedError(JSON.parse(err)))
      })
    })
  }

  // -------- Auth

  setAuthState(authState: AuthState): Promise<AuthState> {
    if (typeof authState === 'object') {
      return new Promise((resolve) => {
        SetAuthState(this.clientId, JSON.stringify(authState), (data) => {
          resolve(JSON.parse(data))
        })
      })
    } else {
      throw new Error('Invalid auth() arguments')
    }
  }

  clearAuthState(): Promise<AuthState> {
    return this.setAuthState({})
  }
}
