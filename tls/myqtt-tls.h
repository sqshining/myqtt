/* 
 *  MyQtt: A high performance open source MQTT implementation
 *  Copyright (C) 2016 Advanced Software Production Line, S.L.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA
 *  
 *  You may find a copy of the license under this software is released
 *  at COPYING file. This is LGPL software: you are welcome to develop
 *  proprietary applications using this library without any royalty or
 *  fee but returning back any change, improvement or addition in the
 *  form of source code, project image, documentation patches, etc.
 *
 *  For commercial support on build MQTT enabled solutions contact us:
 *          
 *      Postal address:
 *         Advanced Software Production Line, S.L.
 *         C/ Antonio Suarez Nº 10, 
 *         Edificio Alius A, Despacho 102
 *         Alcalá de Henares 28802 (Madrid)
 *         Spain
 *
 *      Email address:
 *         info@aspl.es - http://www.aspl.es/mqtt
 *                        http://www.aspl.es/myqtt
 */
#ifndef __MYQTT_TLS_H__
#define __MYQTT_TLS_H__

#include <myqtt.h>

/** 
 * \addtogroup myqtt_tls
 * @{
 */

BEGIN_C_DECLS

/** 
 * @brief Digest method provided.
 */
typedef enum {
	/** 
	 * @brief Allows to especify the DIGEST method SHA-1.
	 */
	MYQTT_SHA1 = 1,
	/** 
	 * @brief Allows to especify the DIGEST method MD5.
	 */
	MYQTT_MD5 = 2,
	/** 
	 * @internal Internal value. Do not modify.
	 */
	MYQTT_DIGEST_NUM
} MyQttDigestMethod;


/** 
 * @brief Handler definition for those functions that allows to locate
 * the certificate file to be used while enabling TLS support.
 * 
 * Once a TLS negotiation is started at least two files are required
 * to enable TLS cyphering: the certificate and the private key. Two
 * handlers are used by MyQtt to allow user app level to configure
 * file locations for both files.
 * 
 * This handler is used to configure location for the certificate
 * file. The function will receive the connection where the TLS is
 * being request to be activated and the serverName value which hold a
 * optional host name value requesting to act as the server configured
 * by this value if SNI indication is received.
 * 
 * The function must return a path to the certificate using a
 * dynamically allocated value or the content of the certificate
 * itself. Once finished, MyQtt will unref it.
 * 
 * <b>The function should return a basename file avoiding full path file
 * names</b>. This is because the MyQtt will use \ref
 * myqtt_support_find_data_file function to locate the file
 * provided. That function is configured to lookup on the configured
 * search path provided by \ref myqtt_support_add_search_path or \ref
 * myqtt_support_add_search_path_ref.
 * 
 * As a consequence: 
 * 
 * - If all certificate files are located at
 *  <b>/etc/repository/certificates</b> and the <b>serverName.cert</b> is to
 *   be used <b>DO NOT</b> return on this function <b>/etc/repository/certificates/serverName.cert</b>
 *
 * - Instead, configure <b>/etc/repository/certificates</b> at \ref
 *    myqtt_support_add_search_path and return <b>servername.cert</b>.
 * 
 * - Doing previous practice will allow your code to be as
 *   platform/directory-structure independent as possible. The same
 *   function works on every installation, the only question to be
 *   configured are the search paths to lookup.
 *
 * @param ctx The context where the operation is taking place.
 * 
 * @param connection The connection where the TLS negotiation was
 * received.
 *
 * @param serverName An optional value requesting to act as the server
 * <b>serverName</b>. This value is supposed to be used to select the
 * right certificate file (according to the common value stored on
 * it).
 *
 * @param user_data Optional reference to user data configured at \ref myqtt_tls_listener_set_certificate_handlers
 * 
 * @return A newly allocated value containing the path to the
 * certificate file or the certificate content to be used.
 */
typedef char  * (* MyQttTlsCertificateFileLocator) (MyQttCtx     * ctx,
						    MyQttConn    * connection,
						    const char   * serverName,
						    axlPointer     user_data);

/** 
 * @brief Handler definition for those functions that allows to locate
 * the private key file to be used while enabling TLS support.
 * 
 * See \ref MyQttTlsCertificateFileLocator handler for more
 * information. This handler allows to define how is located the
 * private key file used for the session TLS activation.
 *
 * @param ctx The context where the operation is taking place.
 *
 * @param connection The connection where the operation is taking place.
 *
 * @param serverName The serverName was has been announced.
 *
 * @param user_data Optional reference to user data configured at \ref myqtt_tls_listener_set_certificate_handlers
 * 
 * @return A newly allocated value containing the path to the private
 * key file or the private key content to be used.
 */
typedef char  * (* MyQttTlsPrivateKeyFileLocator) (MyQttCtx    * ctx,
						   MyQttConn   * connection,
						   const char  * serverName,
						   axlPointer    user_data);

/** 
 * @brief Handler definition for those functions that allows to locate
 * the chain/intermediate certificate file to be used while enabling
 * TLS support.
 * 
 * See \ref MyQttTlsCertificateFileLocator handler for more
 * information. This handler allows to define how is located the
 * private key file used for the session TLS activation.
 *
 *
 * @param ctx The context where the operation is taking place.
 *
 * @param connection The connection where the operation is taking place.
 *
 * @param serverName The serverName was has been announced.
 *
 * @param user_data Optional reference to user data configured at \ref myqtt_tls_listener_set_certificate_handlers
 * 
 * @return A newly allocated value containing the path to the private
 * key file or the private key content to be used.
 */
typedef char  * (* MyQttTlsChainCertificateFileLocator) (MyQttCtx    * ctx,
							 MyQttConn   * connection,
							 const char  * serverName,
							 axlPointer    user_data);

/** 
 * @brief Allows to configure a post-condition function to be executed
 * to perform additional checkings.
 *
 * This handler is used by:
 * 
 *  - \ref myqtt_tls_set_post_check
 *  - \ref myqtt_tls_set_default_post_check
 *
 * The function must return axl_true to signal that checkings was
 * passed, otherwise axl_false must be returned. In such case, the
 * connection will be dropped.
 * 
 * @param conn The connection that was TLS-fixated and
 * additional checks were configured.
 * 
 * @param user_data User defined data passed to the function, defined
 * at \ref myqtt_tls_set_post_check and \ref
 * myqtt_tls_set_default_post_check.
 *
 * @param ssl The SSL object created for the process.
 * 
 * @param ctx The SSL_CTX object created for the process.
 * 
 * @return axl_true to accept the connection, otherwise, axl_false must be
 * returned.
 */
typedef axl_bool  (*MyQttTlsPostCheck) (MyQttConn * conn, 
					axlPointer  user_data, 
					axlPointer  ssl, 
					axlPointer  ctx);


/** 
 * @brief Handler called when a failure is found during TLS
 * handshake. 
 *
 * The function receives the connection where the failure * found an
 * error message and a pointer configured by the user at \ref myqtt_tls_set_failure_handler.
 *
 * @param conn The connection where the failure was found.
 *
 * @param error_message The error message describing the problem found.
 *
 * @param user_data Optional user defined pointer.
 *
 * To get particular SSL info, you can use the following code inside the handler:
 * \code
 * // error variables
 * char          log_buffer [512];
 * unsigned long err;
 *
 * // show errors found
 * while ((err = ERR_get_error()) != 0) {
 *     ERR_error_string_n (err, log_buffer, sizeof (log_buffer));
 *     printf ("tls stack: %s (find reason(code) at openssl/ssl.h)", log_buffer);
 * }
 * \endcode
 * 
 *
 */
typedef void      (*MyQttTlsFailureHandler) (MyQttConn   * conn,
					     const char  * error_message,
					     axlPointer    user_data);


/** 
 * @brief An optional handler that allows user land code to define how
 * is SSL_CTX (SSL context) created and which are the settings it
 * should have before taking place SSL/TLS handshake.
 *
 * NOTE: that the function should return one context for every
 * connection created. Do not reuse unless you know what you are
 * doing.
 *
 * A very bare implementation for this context creator will be:
 *
 * \code 
 * SSL_CTX * my_ssl_ctx_creator (MyQttCtx * ctx, MyQttConn * conn, MyQttConnOpts * opts, axl_bool is_client, axlPointer user_data)
 * {
 *        // very basic context creator using default settings provided by OpenSSL
 *        return SSL_CTX_new (is_client ? TLSv1_client_method () : TLSv1_server_method ()); 
 * }
 * \endcode
 *
 * @param ctx The context where the operation is taking place.
 *
 * @param conn The connection that is being requested for a new context (SSL_CTX). Use is_client to know if this is a connecting client or a listener connection.
 *
 * @param opts Optional reference to the connection object created for this connection.
 *
 * @param is_client axl_true to signal that this is a request for a context for a client connection. Otherwise, it is for a listener connection.
 *
 * @param user_data User defined pointer that received on this function as defined at \ref myqtt_tls_set_ssl_context_creator.
 *
 * @return The function must return a valid SSL_CTX object (see OpenSSL documentation to know more about this) or NULL if it fails.
 */
typedef axlPointer (*MyQttSslContextCreator) (MyQttCtx       * ctx, 
					      MyQttConn      * conn, 
					      MyQttConnOpts  * opts, 
					      axl_bool         is_client, 
					      axlPointer       user_data);

/** 
 * @brief Optional user defined handler that allows to execute SSL
 * post checks code before proceed.
 *
 * This handler allows to implement custom actions while additional
 * verifications about certificate received, validation based on
 * certain attributes, etc.
 *
 * Note that when this handler is called, the SSL handshake has
 * finished without error. In case of SSL handshake failure, this
 * handler is not executed.
 *
 * @param ctx The context where the operation happens.
 *
 * @param conn The connection where the operation takes place and for which the post SSL check is being done.
 *
 * @param SSL_CTX The OpenSSL SSL_CTX object created for this connection.
 *
 * @param SSL The OpenSSL SSL object created for this connection.
 *
 * @param user_data User defined data that is received on this handler.
 */
typedef axl_bool (*MyQttSslPostCheck) (MyQttCtx      * ctx,
				       MyQttConn     * conn,
				       axlPointer      SSL_CTX,
				       axlPointer      SSL,
				       axlPointer      user_data);

/** 
 * @brief SSL/TLS protocol type to use for the client or listener
 * connection. 
 */
typedef enum { 
	/** 
	 * @brief Allows to define SSLv23 as SSL protocol used by the
	 * client or server connection. A TLS/SSL connection
	 * established with these methods may understand SSLv3, TLSv1,
	 * TLSv1.1 and TLSv1.2 protocols (\ref MYQTT_METHOD_SSLV3, \ref MYQTT_METHOD_TLSV1, ...)
	 */
	MYQTT_METHOD_SSLV23      = 2,
	/** 
	 * @brief Allows to define SSLv3 as SSL protocol used by the
	 * client or server connection. A connection/listener
	 * established with this method will only understand this
	 * method.
	 */
	MYQTT_METHOD_SSLV3       = 3,
	/** 
	 * @brief Allows to define TLSv1 as SSL protocol used by the
	 * client or server connection. A connection/listener
	 * established with this method will only understand this
	 * method.
	 */
	MYQTT_METHOD_TLSV1       = 4,
#if defined(TLSv1_1_client_method)
	/** 
	 * @brief Allows to define TLSv1.1 as SSL protocol used by the
	 * client or server connection. A connection/listener
	 * established with this method will only understand this
	 * method.
	 */
	MYQTT_METHOD_TLSV1_1     = 5
#endif
} MyQttSslProtocol ;

axl_bool           myqtt_tls_init                       (MyQttCtx             * ctx);

void               myqtt_tls_set_ssl_context_creator    (MyQttCtx                * ctx,
							 MyQttSslContextCreator    context_creator,
							 axlPointer                user_data);

MyQttConn        * myqtt_tls_conn_new                   (MyQttCtx        * ctx,
							 const char      * client_identifier,
							 axl_bool          clean_session,
							 int               keep_alive,
							 const char      * host, 
							 const char      * port,
							 MyQttConnOpts   * opts,
							 MyQttConnNew      on_connected, 
							 axlPointer        user_data);

MyQttConn        * myqtt_tls_conn_new6                  (MyQttCtx       * ctx,
							 const char     * client_identifier,
							 axl_bool         clean_session,
							 int              keep_alive,
							 const char     * host, 
							 const char     * port,
							 MyQttConnOpts  * opts,
							 MyQttConnNew     on_connected, 
							 axlPointer       user_data);

void              myqtt_tls_listener_set_certificate_handlers (MyQttCtx                            * ctx,
							       MyQttTlsCertificateFileLocator        certificate_handler,
							       MyQttTlsPrivateKeyFileLocator         private_key_handler,
							       MyQttTlsChainCertificateFileLocator   chain_handler,
							       axlPointer                            user_data);

void               myqtt_tls_opts_ssl_peer_verify        (MyQttConnOpts * opts, axl_bool verify);

axl_bool           myqtt_tls_opts_set_ssl_certs          (MyQttConnOpts * opts, 
							  const char     * certificate,
							  const char     * private_key,
							  const char     * chain_certificate,
							  const char     * ca_certificate);

void               myqtt_tls_opts_set_server_name        (MyQttConnOpts * opts,
							  const char    * serverName);

MyQttConn        * myqtt_tls_listener_new                (MyQttCtx             * ctx,
							  const char           * host, 
							  const char           * port, 
							  MyQttConnOpts        * opts,
							  MyQttListenerReady     on_ready, 
							  axlPointer             user_data);

MyQttConn        * myqtt_tls_listener_new6               (MyQttCtx             * ctx,
							  const char           * host, 
							  const char           * port, 
							  MyQttConnOpts        * opts,
							  MyQttListenerReady     on_ready, 
							  axlPointer             user_data);

axl_bool           myqtt_tls_set_certificate            (MyQttConn  * listener,
							 const char * certificate,
							 const char * private_key,
							 const char * chain_file);
	
axl_bool           myqtt_tls_is_on                      (MyQttConn            * conn);

void               myqtt_tls_set_post_check             (MyQttConn             * connection,
							  MyQttTlsPostCheck      post_check,
							  axlPointer             user_data);

void               myqtt_tls_set_default_post_check     (MyQttCtx              * ctx, 
							  MyQttTlsPostCheck      post_check,
							  axlPointer             user_data);

void               myqtt_tls_set_failure_handler        (MyQttCtx                * ctx,
							 MyQttTlsFailureHandler   failure_handler,
							 axlPointer               user_data);

axl_bool           myqtt_tls_verify_cert                (MyQttConn * connection);

axlPointer         myqtt_tls_get_ssl_object             (MyQttConn * connection);

char             * myqtt_tls_get_peer_ssl_digest        (MyQttConn   * connection, 
							 MyQttDigestMethod   method);

char             * myqtt_tls_get_digest                 (MyQttDigestMethod   method,
							 const char         * string);

char             * myqtt_tls_get_digest_sized           (MyQttDigestMethod    method,
							 const char         * content,
							 int                  content_size);

void               myqtt_tls_cleanup                    (void);

#endif

/** 
 * @}
 */
