/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010 Facebook, Inc. (http://www.facebook.com)          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#ifndef __HTTP_SERVER_TRANSPORT_H__
#define __HTTP_SERVER_TRANSPORT_H__

#include <util/base.h>
#include <util/compression.h>
#include <cpp/base/types.h>
#include <cpp/base/type_array.h>

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

/**
 * For storing headers and cookies.
 */
typedef std::map<std::string, std::vector<std::string>, stdltistr> HeaderMap;
typedef std::map<std::string, std::string, stdltistr> CookieMap;

/**
 * A class defining an interface that request handler can use to query
 * transport related information.
 *
 * Note that one transport object is created for each request, and
 * one transport is ONLY accessed from one single thread.
 */
class Transport {
public:
  enum Method {
    UnknownMethod,

    GET,
    POST,
    HEAD,
    AUTO, // check GET parameter first, then POST
  };

  // TODO: add all status codes
  // (http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html)
  enum StatusCode {
    UnknownStatusCode,

    // Success
    OK = 200,

    // Redirection
    MOVED_PERMANENTLY = 301,

    // Client Error
    BAD_REQUEST = 400,
    FORBIDDEN = 403,
    NOT_FOUND = 404,

    // Server Error
    INTERNAL_SERVER_ERROR = 500,
    SERVICE_UNAVAILABLE = 503,
  };

public:
  Transport();
  virtual ~Transport();

  ///////////////////////////////////////////////////////////////////////////
  // Functions sub-classes have to implement.

  /**
   * Request URI.
   */
  virtual const char *getUrl() = 0;
  virtual const char *getRemoteHost() = 0;

  /**
   * POST request's data.
   */
  virtual const void *getPostData(int &size) = 0;

  /**
   * Is this a GET, POST or anything?
   */
  virtual Method getMethod() = 0;
  virtual const char *getExtendedMethod() { return NULL;}

  /**
   * What version of HTTP was the request?
   */
  virtual std::string getHTTPVersion() const;

  /**
   * Get request header(s).
   */
  virtual std::string getHeader(const char *name) = 0;
  virtual void getHeaders(HeaderMap &headers) = 0;

  /**
   * Get/set response headers.
   */
  void addHeader(const char *name, const char *value);
  void addHeader(CStrRef header);
  void replaceHeader(const char *name, const char *value);
  void replaceHeader(CStrRef header);
  void getResponseHeaders(HeaderMap &headers);

  /**
   * Content/MIME type related functions.
   */
  void setMimeType(CStrRef mimeType) { m_mimeType = mimeType.data();}
  String getMimeType() { return String(m_mimeType);}
  const char *getDefaultContentType() { return "text/html";}
  bool sendDefaultContentType() { return m_sendContentType;}
  void setDefaultContentType(bool send) { m_sendContentType = send;}

  /**
   * Can we gzip response?
   */
  void enableCompression() { m_compression = true;}
  void disableCompression() { m_compression = false;}
  bool isCompressionEnabled() const { return m_compression;}

  /**
   * Set cookie response header.
   */
  bool setCookie(CStrRef name, CStrRef value, int expire = 0,
                 CStrRef path = "", CStrRef domain = "", bool secure = false,
                 bool httponly = false, bool encode_url = true);

  /**
   * Add/remove a response header.
   */
  virtual void addHeaderImpl(const char *name, const char *value) = 0;
  virtual void removeHeaderImpl(const char *name) = 0;

  /**
   * Add/remove a request header. Default is no-op, because not all transports
   * need to support incoming request header manipulations.
   */
  virtual void addRequestHeaderImpl(const char *name, const char *value) {}
  virtual void removeRequestHeaderImpl(const char *name) {}

  /**
   * Called when all sending should be done by this time point. Designed for
   * sending last chunk of response for chunked encoding.
   */
  void onSendEnd();

  /**
   * Send back a response with specified code.
   * Caller deletes data, callee must copy
   */
  virtual void sendImpl(const void *data, int size, int code,
                        bool chunked) = 0;

  /**
   * Override to implement more send end logic.
   */
  virtual void onSendEndImpl() {}

  /**
   * Need this implementation to break keep-alive connections.
   */
  virtual bool isServerStopping() { return false;}

  ///////////////////////////////////////////////////////////////////////////
  // Pre-implemented utitlity functions.

  /**
   * We define a "server object" as the part of URL without domain name:
   *
   *   http://facebook.com/foo?x=1       server object is "foo?x=1"
   *   http://facebook.com/foo/bar?x=1   server object is "foo/bar?x=1"
   */
  const char *getServerObject();

  /**
   * We define a "command" as the part of URL without parameters:
   *
   *   /foo?x=1      command is "foo"
   *   foo?x=1       command is "foo"
   *   foo/bar?x=1   command is "foo/bar"
   *   /foo/bar?x=1  command is "foo/bar"
   */
  std::string getCommand();

  /**
   * Whether a parameter exists. Normally this is not needed to know, unless
   * "null" is different from an empty string or 0.
   */
  bool paramExists(const char *name, Method method = GET);

  /**
   * Get value of a parameter. Returns empty string is not present.
   */
  std::string getParam(const char *name, Method method = GET);

  /**
   * Turn a string parameter into an integer.
   */
  int getIntParam(const char *name, Method method = GET);

  /**
   * Turn a string parameter into a 64-bit number.
   */
  long long getInt64Param(const char *name, Method method = GET);

  /**
   * Collect multiple string parameters with the same name into "values".
   *
   *   /foo?x=1&x=2&x=3
   */
  void getArrayParam(const char *name, std::vector<std::string> &values,
                     Method method = GET);

  /**
   * Split a string parameter into multiple sub-strings.
   *
   *  /foo?x=1:2:3
   */
  void getSplitParam(const char *name, std::vector<std::string> &values,
                     char delimiter, Method method = GET);

  /**
   * Test whether client accepts a certain encoding.
   */
  bool acceptEncoding(const char *encoding);

  /**
   * Sending back a response.
   */
  void setResponse(int code) { m_responseCode = code;}
  bool headersSent() { return m_headerSent;}
  virtual void sendRaw(void *data, int size, int code = 200, bool compressed = false,
               bool chunked = false);
  void sendString(const char *data, int code = 200, bool compressed = false,
                  bool chunked = false) {
    sendRaw((void*)data, strlen(data), code, compressed, chunked);
  }
  void sendString(const std::string &data, int code = 200,
                  bool compressed = false, bool chunked = false) {
    sendRaw((void*)data.c_str(), data.length(), code, compressed, chunked);
  }
  void redirect(const char *location, int code = 302);

  // TODO: support rfc1867
  bool isUploadedFile(CStrRef filename);
  bool moveUploadedFile(CStrRef filename, CStrRef destination);

  int getResponseSize() const { return m_responseSize; }
  int getResponseCode() const { return m_responseCode; }

protected:
  /**
   * Parameter parsing in this class is done by making just one copy of the
   * entire query (either URL or post data), then insert termintaing NULLs
   * at end of tokens (name and value), url decode in-place and then store
   * token's start char * addresses in ParamMaps. Therefore, this entire
   * process is very efficient without excessive string copying.
   */
  typedef __gnu_cxx::hash_map<const char *, std::vector<const char *>,
                              __gnu_cxx::hash<const char *>, eqstr> ParamMap;

  // input
  char *m_url;
  char *m_postData;
  bool m_postDataParsed;
  ParamMap m_getParams;
  ParamMap m_postParams;

  // output
  bool m_chunkedEncoding;
  bool m_headerSent;
  int m_responseCode;
  HeaderMap m_responseHeaders;
  CookieMap m_responseCookies;
  int m_responseSize;

  std::string m_mimeType;
  bool m_sendContentType;
  bool m_compression;
  StreamCompressor *m_compressor;

  // helpers
  void parseGetParams();
  void parsePostParams();
  static void parseQuery(char *query, ParamMap &params);
  static void urlUnescape(char *value);
  bool splitHeader(CStrRef header, String &name, const char *&value);

  void prepareHeaders(bool compressed);
  String prepareResponse(const void *data, int size, bool &compressed,
                         bool last);
};

///////////////////////////////////////////////////////////////////////////////
}

#endif // __HTTP_SERVER_TRANSPORT_H__
