import {
  ObserveDataListener,
  ObserveErrorListener,
  CloseObserve,
} from './types'
import { BasedClient } from '.'

function observeListenerToNative(
  onData: ObserveDataListener,
  onError?: ObserveErrorListener
): (data: any, checksum: number, err: any, obsId: number) => void {
  return (data: any, checksum: number, err: any, obsId: number) => {
    if (data) {
      onData(JSON.parse(data), checksum || 0)
    } else if (err && onError) {
      onError(err)
    }
  }
}

const { Observe, Unobserve, Get } =
  require('../build/Release/based-node-addon') as {
    Observe: (
      clientId: number,
      name: string,
      payload: any,
      cb: (data: any, checksum: number, err: any, obsId: number) => void
    ) => number
    Unobserve: (clientId: number, subId: number) => void
    Get: (
      clientId: number,
      name: string,
      payload: any,
      cb: (data: any, err: any, subId: any) => void
    ) => void
  }

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
    const subId = Observe(
      this.client.clientId,
      this.name,
      JSON.stringify(this.query),
      observeListenerToNative(onData, onError)
    )

    return () => {
      Unobserve(this.client.clientId, subId)
    }
  }

  async get(): Promise<K> {
    return new Promise((resolve, reject) => {
      Get(this.client.clientId, this.name, this.query, resolve)
    })
  }
}
