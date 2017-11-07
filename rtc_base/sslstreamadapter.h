/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SSLSTREAMADAPTER_H_
#define RTC_BASE_SSLSTREAMADAPTER_H_

#include <memory>
#include <string>
#include <vector>

#include "rtc_base/sslidentity.h"
#include "rtc_base/stream.h"

namespace rtc {

// Constants for SSL profile.
const int TLS_NULL_WITH_NULL_NULL = 0;

// Constants for SRTP profiles.
const int SRTP_INVALID_CRYPTO_SUITE = 0;
#ifndef SRTP_AES128_CM_SHA1_80
const int SRTP_AES128_CM_SHA1_80 = 0x0001;
#endif
#ifndef SRTP_AES128_CM_SHA1_32
const int SRTP_AES128_CM_SHA1_32 = 0x0002;
#endif
#ifndef SRTP_AEAD_AES_128_GCM
const int SRTP_AEAD_AES_128_GCM = 0x0007;
#endif
#ifndef SRTP_AEAD_AES_256_GCM
const int SRTP_AEAD_AES_256_GCM = 0x0008;
#endif

// Names of SRTP profiles listed above.
// 128-bit AES with 80-bit SHA-1 HMAC.
extern const char CS_AES_CM_128_HMAC_SHA1_80[];
// 128-bit AES with 32-bit SHA-1 HMAC.
extern const char CS_AES_CM_128_HMAC_SHA1_32[];
// 128-bit AES GCM with 16 byte AEAD auth tag.
extern const char CS_AEAD_AES_128_GCM[];
// 256-bit AES GCM with 16 byte AEAD auth tag.
extern const char CS_AEAD_AES_256_GCM[];

// Given the DTLS-SRTP protection profile ID, as defined in
// https://tools.ietf.org/html/rfc4568#section-6.2 , return the SRTP profile
// name, as defined in https://tools.ietf.org/html/rfc5764#section-4.1.2.
std::string SrtpCryptoSuiteToName(int crypto_suite);

// The reverse of above conversion.
int SrtpCryptoSuiteFromName(const std::string& crypto_suite);

// Get key length and salt length for given crypto suite. Returns true for
// valid suites, otherwise false.
bool GetSrtpKeyAndSaltLengths(int crypto_suite, int *key_length,
    int *salt_length);

// Returns true if the given crypto suite id uses a GCM cipher.
bool IsGcmCryptoSuite(int crypto_suite);

// Returns true if the given crypto suite name uses a GCM cipher.
bool IsGcmCryptoSuiteName(const std::string& crypto_suite);

struct CryptoOptions {
  CryptoOptions() {}

  // Helper method to return an instance of the CryptoOptions with GCM crypto
  // suites disabled. This method should be used instead of depending on current
  // default values set by the constructor.
  static CryptoOptions NoGcm();

  // Enable GCM crypto suites from RFC 7714 for SRTP. GCM will only be used
  // if both sides enable it.
  bool enable_gcm_crypto_suites = false;

  // If set to true, encrypted RTP header extensions as defined in RFC 6904
  // will be negotiated. They will only be used if both peers support them.
  bool enable_encrypted_rtp_header_extensions = false;
};

// Returns supported crypto suites, given |crypto_options|.
// CS_AES_CM_128_HMAC_SHA1_32 will be preferred by default.
std::vector<int> GetSupportedDtlsSrtpCryptoSuites(
    const rtc::CryptoOptions& crypto_options);

// SSLStreamAdapter : A StreamInterfaceAdapter that does SSL/TLS.
// After SSL has been started, the stream will only open on successful
// SSL verification of certificates, and the communication is
// encrypted of course.
//
// This class was written with SSLAdapter as a starting point. It
// offers a similar interface, with two differences: there is no
// support for a restartable SSL connection, and this class has a
// peer-to-peer mode.
//
// The SSL library requires initialization and cleanup. Static method
// for doing this are in SSLAdapter. They should possibly be moved out
// to a neutral class.


enum SSLRole { SSL_CLIENT, SSL_SERVER };
enum SSLMode { SSL_MODE_TLS, SSL_MODE_DTLS };
enum SSLProtocolVersion {
  SSL_PROTOCOL_TLS_10,
  SSL_PROTOCOL_TLS_11,
  SSL_PROTOCOL_TLS_12,
  SSL_PROTOCOL_DTLS_10 = SSL_PROTOCOL_TLS_11,
  SSL_PROTOCOL_DTLS_12 = SSL_PROTOCOL_TLS_12,
};
enum class SSLPeerCertificateDigestError {
  NONE,
  UNKNOWN_ALGORITHM,
  INVALID_LENGTH,
  VERIFICATION_FAILED,
};

// Errors for Read -- in the high range so no conflict with OpenSSL.
enum { SSE_MSG_TRUNC = 0xff0001 };

// Used to send back UMA histogram value. Logged when Dtls handshake fails.
enum class SSLHandshakeError { UNKNOWN, INCOMPATIBLE_CIPHERSUITE, MAX_VALUE };

class SSLStreamAdapter : public StreamAdapterInterface {
 public:
  // Instantiate an SSLStreamAdapter wrapping the given stream,
  // (using the selected implementation for the platform).
  // Caller is responsible for freeing the returned object.
  static SSLStreamAdapter* Create(StreamInterface* stream);

  explicit SSLStreamAdapter(StreamInterface* stream);
  ~SSLStreamAdapter() override;

  void set_ignore_bad_cert(bool ignore) { ignore_bad_cert_ = ignore; }
  bool ignore_bad_cert() const { return ignore_bad_cert_; }

  void set_client_auth_enabled(bool enabled) { client_auth_enabled_ = enabled; }
  bool client_auth_enabled() const { return client_auth_enabled_; }

  // Specify our SSL identity: key and certificate. SSLStream takes ownership
  // of the SSLIdentity object and will free it when appropriate. Should be
  // called no more than once on a given SSLStream instance.
  virtual void SetIdentity(SSLIdentity* identity) = 0;

  // Call this to indicate that we are to play the server role (or client role,
  // if the default argument is replaced by SSL_CLIENT).
  // The default argument is for backward compatibility.
  // TODO(ekr@rtfm.com): rename this SetRole to reflect its new function
  virtual void SetServerRole(SSLRole role = SSL_SERVER) = 0;

  // Do DTLS or TLS.
  virtual void SetMode(SSLMode mode) = 0;

  // Set maximum supported protocol version. The highest version supported by
  // both ends will be used for the connection, i.e. if one party supports
  // DTLS 1.0 and the other DTLS 1.2, DTLS 1.0 will be used.
  // If requested version is not supported by underlying crypto library, the
  // next lower will be used.
  virtual void SetMaxProtocolVersion(SSLProtocolVersion version) = 0;

  // Set the initial retransmission timeout for DTLS messages. When the timeout
  // expires, the message gets retransmitted and the timeout is exponentially
  // increased.
  // This should only be called before StartSSL().
  virtual void SetInitialRetransmissionTimeout(int timeout_ms) = 0;

  // StartSSL starts negotiation with a peer, whose certificate is verified
  // using the certificate digest. Generally, SetIdentity() and possibly
  // SetServerRole() should have been called before this.
  // SetPeerCertificateDigest() must also be called. It may be called after
  // StartSSLWithPeer() but must be called before the underlying stream opens.
  //
  // Use of the stream prior to calling StartSSL will pass data in clear text.
  // Calling StartSSL causes SSL negotiation to begin as soon as possible: right
  // away if the underlying wrapped stream is already opened, or else as soon as
  // it opens.
  //
  // StartSSL returns a negative error code on failure. Returning 0 means
  // success so far, but negotiation is probably not complete and will continue
  // asynchronously. In that case, the exposed stream will open after
  // successful negotiation and verification, or an SE_CLOSE event will be
  // raised if negotiation fails.
  virtual int StartSSL() = 0;

  // Specify the digest of the certificate that our peer is expected to use.
  // Only this certificate will be accepted during SSL verification. The
  // certificate is assumed to have been obtained through some other secure
  // channel (such as the signaling channel). This must specify the terminal
  // certificate, not just a CA. SSLStream makes a copy of the digest value.
  //
  // Returns true if successful.
  // |error| is optional and provides more information about the failure.
  virtual bool SetPeerCertificateDigest(
      const std::string& digest_alg,
      const unsigned char* digest_val,
      size_t digest_len,
      SSLPeerCertificateDigestError* error = nullptr) = 0;

  // Retrieves the peer's X.509 certificate, if a connection has been
  // established. It returns the transmitted over SSL, including the entire
  // chain.
  virtual std::unique_ptr<SSLCertificate> GetPeerCertificate() const = 0;

  // Retrieves the IANA registration id of the cipher suite used for the
  // connection (e.g. 0x2F for "TLS_RSA_WITH_AES_128_CBC_SHA").
  virtual bool GetSslCipherSuite(int* cipher_suite);

  virtual int GetSslVersion() const = 0;

  // Key Exporter interface from RFC 5705
  // Arguments are:
  // label               -- the exporter label.
  //                        part of the RFC defining each exporter
  //                        usage (IN)
  // context/context_len -- a context to bind to for this connection;
  //                        optional, can be null, 0 (IN)
  // use_context         -- whether to use the context value
  //                        (needed to distinguish no context from
  //                        zero-length ones).
  // result              -- where to put the computed value
  // result_len          -- the length of the computed value
  virtual bool ExportKeyingMaterial(const std::string& label,
                                    const uint8_t* context,
                                    size_t context_len,
                                    bool use_context,
                                    uint8_t* result,
                                    size_t result_len);

  // DTLS-SRTP interface
  virtual bool SetDtlsSrtpCryptoSuites(const std::vector<int>& crypto_suites);
  virtual bool GetDtlsSrtpCryptoSuite(int* crypto_suite);

  // Returns true if a TLS connection has been established.
  // The only difference between this and "GetState() == SE_OPEN" is that if
  // the peer certificate digest hasn't been verified, the state will still be
  // SS_OPENING but IsTlsConnected should return true.
  virtual bool IsTlsConnected() = 0;

  // Capabilities testing.
  // Used to have "DTLS supported", "DTLS-SRTP supported" etc. methods, but now
  // that's assumed.
  static bool IsBoringSsl();

  // Returns true iff the supplied cipher is deemed to be strong.
  // TODO(torbjorng): Consider removing the KeyType argument.
  static bool IsAcceptableCipher(int cipher, KeyType key_type);
  static bool IsAcceptableCipher(const std::string& cipher, KeyType key_type);

  // TODO(guoweis): Move this away from a static class method. Currently this is
  // introduced such that any caller could depend on sslstreamadapter.h without
  // depending on specific SSL implementation.
  static std::string SslCipherSuiteToName(int cipher_suite);

  // Use our timeutils.h source of timing in BoringSSL, allowing us to test
  // using a fake clock.
  static void enable_time_callback_for_testing();

  sigslot::signal1<SSLHandshakeError> SignalSSLHandshakeError;

 private:
  // If true, the server certificate need not match the configured
  // server_name, and in fact missing certificate authority and other
  // verification errors are ignored.
  bool ignore_bad_cert_;

  // If true (default), the client is required to provide a certificate during
  // handshake. If no certificate is given, handshake fails. This applies to
  // server mode only.
  bool client_auth_enabled_;
};

}  // namespace rtc

#endif  // RTC_BASE_SSLSTREAMADAPTER_H_
