import {
  ObserveDataListener,
  ObserveErrorListener,
  CloseObserve,
} from './types'
import { BasedClient } from '.'

const {
  native_observe,
  native_unobserve,
  native_get,
} = require('../build/Release/based-node-addon')

export class BasedQuery<P = any, K = any> {
  public query: P
  public name: string
  public client: BasedClient

  constructor(client: BasedClient, name: string, payload: P) {
    this.query = payload
    this.client = client
    this.name = name
  }

  subscribe(
    onData: ObserveDataListener<K>,
    onError?: ObserveErrorListener
  ): CloseObserve {
    const subId = native_observe(
      this.client.clientId,
      this.name,
      this.query,
      onData
    )

    return () => {
      native_unobserve(this.client.clientId, subId)
    }
  }

  async get(): Promise<K> {
    return new Promise((resolve, reject) => {
      native_get(this.client.clientId, this.name, this.query, resolve)
    })
  }
}
