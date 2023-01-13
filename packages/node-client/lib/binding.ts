const addon = require("../build/Release/hello-native");

export = {
  GetService: addon.GetService,
  Connect: addon.Connect,
  Function: addon.Function,
  ConnectToUrl: addon.ConnectToUrl,
  Observe: addon.Observe,
  Unobserve: addon.Unobserve,
  Get: addon.Get,
};
