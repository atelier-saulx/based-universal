import { BasedClient } from './'
import { ChannelMessageFunction } from './types/channel'
import { BasedError } from './types/error'

const { ChannelSubscribe, ChannelUnsubscribe, ChannelPublish } =
  require('../build/Release/based-node-addon') as {
    ChannelSubscribe: (
      clientId: number,
      name: string,
      payload: string,
      cb: (data: any, err: any, reqId: number) => void
    ) => number
    ChannelUnsubscribe: (clientId: number, reqId: number) => void
    ChannelPublish: (
      clientId: number,
      name: string,
      payload: string,
      message: string
    ) => void
  }

export class BasedChannel<P = any, K = any> {
  public client: BasedClient
  public name: string
  public payload: P

  constructor(client: BasedClient, name: string, payload: P) {
    this.client = client
    this.name = name
    this.payload = payload
  }

  subscribe(
    onMessage: ChannelMessageFunction,
    onError?: (err: BasedError) => void
  ): () => void {
    const id = ChannelSubscribe(
      this.client.clientId,
      this.name,
      JSON.stringify(this.payload),
      (data, err, reqId) => {
        if (data) onMessage(JSON.parse(data))
        else if (onError && err) onError(JSON.parse(err))
      }
    )

    return () => {
      ChannelUnsubscribe(this.client.clientId, id)
    }
  }

  publish(message: K): void {
    ChannelPublish(
      this.client.clientId,
      this.name,
      JSON.stringify(this.payload),
      JSON.stringify(message)
    )
  }
}
