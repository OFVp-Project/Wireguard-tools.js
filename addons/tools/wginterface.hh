#pragma once
#include <napi.h>
#include <string>
#include <vector>
#include <map>

int maxName();

/*
Esta função consegela o loop event

Pegar todas as interfaces do Wireguard e retorna em forma de String sendo tratado pelo Node.js quando retornado.
*/
Napi::Value listDevicesSync(const Napi::CallbackInfo& info);

class Peer {
  public:
  bool removeMe;
  std::string presharedKey;
  std::string endpoint;
  std::vector<std::string> allowedIPs;
  uint32_t keepInterval;
  uint64_t rxBytes, txBytes, last_handshake;
};

/*
Configure uma interface do Wireguard.
*/
class setConfig : public Napi::AsyncWorker {
  private:
    // Wireguard interface name (required)
    std::string wgName;

    // Wireguard private key (required)
    std::string privateKey;

    // Wireguard interface publicKey <optional>
    std::string publicKey;

    // Wireguard port listen
    uint32_t portListen;

    // Wiki
    uint32_t fwmark;

    // Interface address'es
    std::vector<std::string> Address;

    // Replace peers
    bool replacePeers;

    /*
    Wireguard peers
    Map: <publicKey, Peer>
    */
    std::map<std::string, Peer> peersVector;
  public:
  void OnOK() override {
    Napi::HandleScope scope(Env());
    Callback().Call({ Env().Undefined() });
  };
  void OnError(const Napi::Error& e) override {
    Napi::HandleScope scope(Env());
    Callback().Call({ e.Value() });
  }

  ~setConfig() {}
  setConfig(const Napi::Env env, const Napi::Function &callback, std::string name, const Napi::Object &config) : AsyncWorker(callback), wgName(name) {
    // Wireguard public key
    const auto sppk = config.Get("publicKey");
    if (sppk.IsString()) publicKey = sppk.ToString().Utf8Value();

    // Private key
    const auto sprk = config.Get("privateKey");
    if (!(sprk.IsString())) throw Napi::Error::New(env, "privateKey is empty");
    privateKey = sprk.ToString().Utf8Value();

    // Port to listen Wireguard interface
    const auto spor = config.Get("portListen");
    if (spor.IsNumber() && (spor.ToNumber().Int32Value() > 0)) portListen = spor.ToNumber().Int32Value();

    //\?
    const auto sfw = config.Get("fwmark");
    if (sfw.IsNumber() && (sfw.ToNumber().Uint32Value() >= 0)) fwmark = sfw.ToNumber().Uint32Value();

    const auto saddr = config.Get("Address");
    if (saddr.IsArray()) {
      const Napi::Array addrs = saddr.As<Napi::Array>();
      for (unsigned int i = 0; i < addrs.Length(); i++) {
        if (addrs.Get(i).IsString()) Address.push_back(addrs.Get(i).ToString().Utf8Value());
      }
    }

    // Replace peers
    const auto srpee = config.Get("replacePeers");
    if (srpee.IsBoolean()) replacePeers = srpee.ToBoolean().Value();

    // Peers
    const auto speers = config.Get("peers");
    if (speers.IsObject()) {
      const Napi::Object Peers = speers.ToObject();
      const Napi::Array Keys = Peers.GetPropertyNames();
      for (unsigned int peerIndex = 0; peerIndex < Keys.Length(); peerIndex++) {
        const auto peerPubKey = Keys[peerIndex];
        if (peerPubKey.IsString() && Peers.Get(Keys[peerIndex]).IsObject()) {
          std::string ppkey = peerPubKey.ToString().Utf8Value();
          const Napi::Object peerConfigObject = Peers.Get(Keys[peerIndex]).ToObject();

          Peer peerConfig = Peer();
          const auto removeMe = peerConfigObject.Get("removeMe");
          if (removeMe.IsBoolean() && removeMe.ToBoolean().Value()) peerConfig.removeMe = true;
          else {
            // Preshared key
            const auto pprekey = peerConfigObject.Get("presharedKey");
            if (pprekey.IsString()) peerConfig.presharedKey = pprekey.ToString().Utf8Value();

            // Keep interval
            const auto pKeepInterval = peerConfigObject.Get("keepInterval");
            if (pKeepInterval.IsNumber() && (pKeepInterval.ToNumber().Int32Value() > 0)) peerConfig.keepInterval = pKeepInterval.ToNumber().Int32Value();

            // Peer endpoint
            const auto pEndpoint = peerConfigObject.Get("endpoint");
            if (pEndpoint.IsString()) peerConfig.endpoint = pEndpoint.ToString().Utf8Value();

            // Allowed ip's array
            const auto pAllowedIPs = peerConfigObject.Get("allowedIPs");
            if (pAllowedIPs.IsArray()) {
              const auto AllowedIps = pAllowedIPs.As<Napi::Array>();
              for (uint32_t allIndex = 0; allIndex < AllowedIps.Length(); allIndex++) {
                if (AllowedIps.Get(allIndex).IsString()) peerConfig.allowedIPs.push_back(AllowedIps.Get(allIndex).ToString().Utf8Value());
              }
            }
          }

          // Insert peer
          peersVector[ppkey] = peerConfig;
        }
      }
    }
  }

  // Set platform Execute script
  void Execute() override;
};

class getConfig : public Napi::AsyncWorker {
  private:
    // Wireguard interface name (required)
    std::string wgName;

    // Wireguard private key (required)
    std::string privateKey;

    // Wireguard interface publicKey <optional>
    std::string publicKey;

    // Wireguard port listen
    uint32_t portListen;

    // Wiki
    uint32_t fwmark;

    // Interface address'es
    std::vector<std::string> Address;

    // Replace peers
    bool replacePeers;

    /*
    Wireguard peers
    Map: <publicKey, Peer>
    */
    std::map<std::string, Peer> peersVector;
  public:
  void OnError(const Napi::Error& e) override {
    Napi::HandleScope scope(Env());
    Callback().Call({ e.Value() });
  }

  void OnOK() override {
    Napi::HandleScope scope(Env());
    const Napi::Env env = Env();
    const auto config = Napi::Object::New(env);

    if (publicKey.length() > 0) config.Set("publicKey", publicKey);
    if (privateKey.length() > 0) config.Set("privateKey", privateKey);
    if (portListen > 0) config.Set("portListen", portListen);
    if (fwmark >= 0) config.Set("fwmark", fwmark);
    if (Address.size() > 0) {
      const auto Addrs = Napi::Array::New(env);
      for (auto it = Address.begin(); it != Address.end(); ++it) Addrs.Set(Addrs.Length(), it->append(""));
      config.Set("Address", Addrs);
    }

    // Peer object
    const auto PeersObject = Napi::Object::New(env);
    for (auto it = peersVector.begin(); it != peersVector.end(); ++it) {
      const auto PeerObject = Napi::Object::New(env);
      const std::string peerPubKey = it->first;
      auto peerConfig = it->second;

      if (peerConfig.presharedKey.length() > 0) PeerObject.Set("presharedKey", peerConfig.presharedKey);
      if (peerConfig.keepInterval > 0) PeerObject.Set("keepInterval", peerConfig.keepInterval);
      if (peerConfig.endpoint.length() > 0) PeerObject.Set("endpoint", peerConfig.endpoint);
      if (peerConfig.allowedIPs.size() > 0) {
        const auto allowedIPs = Napi::Array::New(env);
        for (auto it = peerConfig.allowedIPs.begin(); it != peerConfig.allowedIPs.end(); ++it) allowedIPs.Set(allowedIPs.Length(), it->append(""));
        PeerObject.Set("allowedIPs", allowedIPs);
      }

      PeersObject.Set(peerPubKey, PeerObject);
    }
    config.Set("peers", PeersObject);

    // Done and callack call
    Callback().Call({ Env().Undefined(), config });
  };

  ~getConfig() {}
  getConfig(const Napi::Function &callback, std::string name): AsyncWorker(callback), wgName(name) {}

  // Set platform Execute script
  void Execute() override;
};