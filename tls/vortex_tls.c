/* 
 *  LibVortex:  A BEEP (RFC3080/RFC3081) implementation.
 *  Copyright (C) 2008 Advanced Software Production Line, S.L.
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
 *  For commercial support on build BEEP enabled solutions contact us:
 *          
 *      Postal address:
 *         Advanced Software Production Line, S.L.
 *         C/ Antonio Suarez Nº 10, 
 *         Edificio Alius A, Despacho 102
 *         Alcalá de Henares 28802 (Madrid)
 *         Spain
 *
 *      Email address:
 *         info@aspl.es - http://www.aspl.es/vortex
 */
/**
 * \defgroup vortex_tls Vortex TLS: TLS profile support and related functions
 */

/**
 * \addtogroup vortex_tls
 * @{
 */

#define LOG_DOMAIN "vortex-tls"

/* include tls header */
#include <vortex_tls.h>

/* some keys to store creation handlers and its associate data */
#define CTX_CREATION      "tls:ctx-creation"
#define CTX_CREATION_DATA "tls:ctx-creation:data"
#define POST_CHECK        "tls:post-checks"
#define POST_CHECK_DATA   "tls:post-checks:data"
#define TLS_CTX           "tls:ctx"



typedef struct _VortexTlsCtx {

	/* @internal Internal default handlers used to define the TLS
	 * profile support. */
	VortexTlsAcceptQuery               tls_accept_handler;
	VortexTlsCertificateFileLocator    tls_certificate_handler;
	VortexTlsPrivateKeyFileLocator     tls_private_key_handler;

	/* default ctx creation */
	VortexTlsCtxCreation               tls_default_ctx_creation;
	axlPointer                         tls_default_ctx_creation_user_data;

	/* default post check */
	VortexTlsPostCheck                 tls_default_post_check;
	axlPointer                         tls_default_post_check_user_data;

	/** 
	 * @internal
	 * @brief Auto TLS profile negotiation internal support variables.
	 */
	bool                               connection_auto_tls;
	bool     	                   connection_auto_tls_allow_failures;
	char  *                            connection_auto_tls_server_name;

} VortexTlsCtx;


/** 
 * @internal
 *
 * @brief Include headers and default global variables declarations if
 * the library build have built-in support.
 * 
 * What we seek is to support applications that are linked to Vortex
 * Library but in a manner that they do not get undefined function
 * references while using a Vortex Library without TLS support.
 *
 * All function defined at this module have especial handling code
 * done to not perform any operation if the library wasn't built with
 * TLS support.
 */
#include <openssl/x509v3.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


/** 
 * @brief Initialize the TLS library.
 * 
 * All client applications using TLS profile must call to this
 * function in order to ensure TLS profile engine has been successfully
 * enabled.
 *
 * @param ctx The context where the operation will be performed.
 *
 * @return true if the TLS profile was initialized. Otherwise false
 * is returned.
 */
bool     vortex_tls_init (VortexCtx * ctx)
{
	VortexTlsCtx * tls_ctx;

	/* check context received */
	v_return_val_if_fail (ctx, false);

	/* check if the tls ctx was created */
	tls_ctx = vortex_ctx_get_data (ctx, TLS_CTX);
	if (tls_ctx != NULL) 
		return true;

	/* create the tls context */
	tls_ctx = axl_new (VortexTlsCtx, 1);
	vortex_ctx_set_data_full (ctx,
				  /* key and value */
				  TLS_CTX, tls_ctx,
				  NULL, axl_free);

	/* install connection action */
	vortex_connection_set_connection_actions (ctx,
						  CONNECTION_STAGE_POST_CREATED,
						  vortex_tls_auto_tlsfixate_conection,
						  NULL);
	/* init ssl ciphers and engines */
	SSL_library_init ();

	return true;
}

/** 
 * @brief Allows to configure the SSL context creation function to be
 * called, once the TLS process is activated, and it is required an
 * SSL_CTX object.
 *
 * See \ref VortexTlsCtxCreation for more information.
 *
 * If you want to configure a global handler to be called for all
 * connections, you can use the default handler: \ref
 * vortex_tls_set_default_ctx_creation.
 *
 * <i>NOTE: Using this function for the server side, will disable the
 * following handlers, (provided at the \ref vortex_tls_accept_negotiation):
 * 
 *  - certificate_handler (\ref VortexTlsCertificateFileLocator)
 *  - private_key_handler (\ref VortexTlsPrivateKeyFileLocator)
 *
 * This is because providing a function to create the SSL context
 * (SSL_CTX) assumes the application layer on top of Vortex Library
 * takes control over the SSL configuration process. This ensures Vortex Library will
 * not do any additional configuration once created the
 * SSL context (SSL_CTX).
 *
 * </i>
 * 
 * @param connection The connection that is going to be TLS-fixated.
 *
 * @param ctx_creation The handler to be called once required a
 * SSL_CTX object.
 *
 * @param user_data User defined data, that is passed to the handler
 * provided (ctx_creation).
 */
void               vortex_tls_set_ctx_creation           (VortexConnection     * connection,
							  VortexTlsCtxCreation   ctx_creation, 
							  axlPointer             user_data)
{
	/* check parameters received */
	if (connection == NULL || ctx_creation == NULL)
		return;

	/* configure the handler */
	vortex_connection_set_data (connection, CTX_CREATION,      ctx_creation);
	vortex_connection_set_data (connection, CTX_CREATION_DATA, user_data);

	return;
}

/** 
 * @brief Allows to configure the default SSL context creation
 * function to be called, once the TLS process is activated, and it is
 * required an SSL_CTX object.
 *
 * See \ref VortexTlsCtxCreation for more information.
 *
 * If you want to configure a per-connection handler to be called for all
 * connections, you can use the following handler: \ref vortex_tls_set_ctx_creation.
 *
 * <i>NOTE: Using this function for the server side, will disable the
 * following handlers, (provided at the \ref vortex_tls_accept_negotiation):
 * 
 *  - certificate_handler (\ref VortexTlsCertificateFileLocator)
 *  - private_key_handler (\ref VortexTlsPrivateKeyFileLocator)
 *
 * This is because providing a function to create the SSL context
 * (SSL_CTX) assumes the application layer on top of Vortex Library
 * takes control over the SSL configuration process. This ensures
 * Vortex Library will not do any additional configure operation once
 * create the SSL context (SSL_CTX).
 *
 * </i>
 * 
 * @param ctx The context where the operation will be performed.
 * 
 * @param ctx_creation The handler to be called once required a
 * SSL_CTX object.
 *
 * @param user_data User defined data, that is passed to the handler
 * provided (ctx_creation).
 */
void               vortex_tls_set_default_ctx_creation   (VortexCtx            * ctx, 
							  VortexTlsCtxCreation   ctx_creation, 
							  axlPointer             user_data)
{
	VortexTlsCtx * tls_ctx;

	/* check parameters received */
	if (ctx == NULL || ctx_creation == NULL)
		return;

	/* check if the tls ctx was created */
	tls_ctx = vortex_ctx_get_data (ctx, TLS_CTX);
	if (tls_ctx == NULL) 
		return;

	/* configure the handler */
	tls_ctx->tls_default_ctx_creation           = ctx_creation;
	tls_ctx->tls_default_ctx_creation_user_data = user_data;

	return;
}

/** 
 * @brief Allows to configure a function that will be executed at the
 * end of the TLS process, before returning the connection to be
 * usable.
 *
 * See \ref VortexTlsPostCheck for more information.
 *
 * If you want to configure a global handler to be called for all
 * connections, you can use the default handler: \ref
 * vortex_tls_set_default_post_check
 * 
 * @param connection The connection to be post-checked.
 *
 * @param post_check The handler to be called once required to perform the post-check.
 *
 * @param user_data User defined data, that is passed to the handler
 * provided (post_check).
 */
void               vortex_tls_set_post_check             (VortexConnection     * connection,
							  VortexTlsPostCheck     post_check,
							  axlPointer             user_data)
{
	/* check parameters received */
	if (connection == NULL || post_check == NULL)
		return;

	/* configure the handler */
	vortex_connection_set_data (connection, POST_CHECK,      post_check);
	vortex_connection_set_data (connection, POST_CHECK_DATA, user_data);

	return;
}

/** 
 * @brief Allows to configure a function that will be executed at the
 * end of the TLS process, before returning the connection to be
 * usable.
 *
 * See \ref VortexTlsPostCheck for more information.
 *
 * If you want to configure a per-connection handler to be called for all
 * connections, you can use the default handler: \ref
 * vortex_tls_set_default_post_check
 *
 * @param ctx The context where the operation will be performed.
 * 
 * @param post_check The handler to be called once required to perform the post-check.
 *
 * @param user_data User defined data, that is passed to the handler
 * provided (post_check).
 */
void               vortex_tls_set_default_post_check     (VortexCtx            * ctx, 
							  VortexTlsPostCheck     post_check,
							  axlPointer             user_data)
{
	VortexTlsCtx * tls_ctx;

	/* check parameters received */
	if (ctx == NULL || post_check == NULL)
		return;

	/* check if the tls ctx was created */
	tls_ctx = vortex_ctx_get_data (ctx, TLS_CTX);
	if (tls_ctx == NULL) 
		return;

	/* configure the handler */
	tls_ctx->tls_default_post_check           = post_check;
	tls_ctx->tls_default_post_check_user_data = user_data;

	return;
}

/** 
 * @internal
 *
 * @brief Default handlers used to actually read from underlying
 * transport while a connection is working under TLS.
 * 
 * @param connection The connection where the read operation will be performed.
 * @param buffer     The buffer where the data read will be returned
 * @param buffer_len Buffer size
 * 
 * @return How many bytes was read.
 */
int  vortex_tls_ssl_read (VortexConnection * connection, char  * buffer, int  buffer_len)
{
	SSL         * ssl;
	VortexMutex * mutex;
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx   * ctx = vortex_connection_get_ctx (connection);
#endif
	int    res;
	int    ssl_err;

	/* get ssl object */
	ssl = vortex_connection_get_data (connection, "ssl-data:ssl");
	if (ssl == NULL) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "unable to find ssl object to read data");
		return 0;
	}

 retry:
	/* get and lock the mutex */
	mutex = vortex_connection_get_data (connection, "ssl-data:mutex");
	if (mutex == NULL) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "unable to find mutex to protect ssl object to read data");
		return 0;
	}
	vortex_mutex_lock (mutex);

	/* read data */
	res = SSL_read (ssl, buffer, buffer_len);

	/* unlock the mutex */
	vortex_mutex_unlock (mutex);

	/* get error returned */
	ssl_err = SSL_get_error(ssl, res);
	switch (ssl_err) {
	case SSL_ERROR_NONE:
		/* no error, return the number of bytes read */
		return res;
	case SSL_ERROR_WANT_WRITE:
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_X509_LOOKUP:
		vortex_log (VORTEX_LEVEL_DEBUG, "SSL_read returned that isn't ready to read: retrying");
		return -2;
	case SSL_ERROR_SYSCALL:
		if(res < 0) { /* not EOF */
			if(errno == VORTEX_EINTR) {
				vortex_log (VORTEX_LEVEL_DEBUG, "SSL read interrupted by a signal: retrying");
				goto retry;
			}
			vortex_log (VORTEX_LEVEL_WARNING, "SSL_read (SSL_ERROR_SYSCALL)");
			return -1;
		}
		vortex_log(VORTEX_LEVEL_DEBUG, "SSL socket closed on SSL_read (res=%d, ssl_err=%d, errno=%d)",
			    res, ssl_err, errno);
		return res;
	case SSL_ERROR_ZERO_RETURN: /* close_notify received */
		vortex_log (VORTEX_LEVEL_DEBUG, "SSL closed on SSL_read");
		return res;
	case SSL_ERROR_SSL:
		vortex_log (VORTEX_LEVEL_WARNING, "SSL_read function error (res=%d, ssl_err=%d, errno=%d)",
			    res, ssl_err, errno);
		return -1;
	default:
		/* nothing to handle */
		break;
	}
	vortex_log (VORTEX_LEVEL_WARNING, "SSL_read/SSL_get_error returned %d", res);
	return -1;
}

/** 
 * @internal
 *
 * @brief Default handlers used to actually write to the underlying
 * transport while a connection is working under TLS.
 * 
 * @param connection The connection where the write operation will be performed.
 * @param buffer     The buffer containing data to be sent.
 * @param buffer_len The buffer size.
 * 
 * @return How many bytes was written.
 */
int  vortex_tls_ssl_write (VortexConnection * connection, const char  * buffer, int  buffer_len)
{
	int           res;
	int           ssl_err;
	VortexMutex * mutex;
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx   * ctx = vortex_connection_get_ctx (connection);
#endif

	SSL * ssl = vortex_connection_get_data (connection, "ssl-data:ssl");
	if (ssl == NULL) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "unable to find ssl object to read data");
		return 0;
	}
	/* try to write */
 retry:
	/* get and lock the mutex */
	mutex = vortex_connection_get_data (connection, "ssl-data:mutex");
	if (mutex == NULL) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "unable to find mutex to protect ssl object to read data");
		return 0;
	}
	vortex_mutex_lock (mutex);
	
	/* write data */
	res = SSL_write (ssl, buffer, buffer_len);

	/* unlock the mutex */
	vortex_mutex_unlock (mutex);
	
	/* get error */
	ssl_err = SSL_get_error(ssl, res);
	switch (ssl_err) {
	case SSL_ERROR_NONE:
		/* no error, nothing to report */
		return res;
	case SSL_ERROR_WANT_WRITE:
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_X509_LOOKUP:
		vortex_log (VORTEX_LEVEL_DEBUG, "SSL_write returned that needs more data : retrying");
		return -2;
	case SSL_ERROR_SYSCALL:
		if(res < 0) { /* really an error */
			if(errno == VORTEX_EINTR) {
				vortex_log (VORTEX_LEVEL_DEBUG, "SSL write interrupted by a signal: retrying");
				goto retry;
			}
			vortex_log (VORTEX_LEVEL_CRITICAL, "SSL_write (ERROR_SYSCALL)");
			return -1;
		}
		break;
	case SSL_ERROR_ZERO_RETURN: /* close_notify received */
		vortex_log (VORTEX_LEVEL_DEBUG, "SSL closed on SSL_write");
		return 0;
	case SSL_ERROR_SSL:
		vortex_log (VORTEX_LEVEL_WARNING, "SSL_write ssl internal error (res=%d, ssl_err=%d, errno=%d)",
			    res, ssl_err, errno);
		return -1;
	default:
		/* nothing to handle */
		break;
	}

	vortex_log (VORTEX_LEVEL_WARNING, "SSL_write/SSL_get_error returned %d", res);
	return -1;
}

/** 
 * @internal Internal function to release and free the mutex memory
 * allocated.
 * 
 * @param mutex The mutex to destroy.
 */
void __vortex_tls_free_mutex (VortexMutex * mutex)
{
	/* free mutex */
	vortex_mutex_destroy (mutex);
	axl_free (mutex);
	return;
}

/** 
 * @brief Common function which sets needed data for the TLS transport
 * and default callbacks for read and write data.
 * 
 * @param connection The connection to configure
 * @param ssl The ssl object.
 * @param _ctx The ssl context
 */
void vortex_tls_set_common_data (VortexConnection * connection, 
				 SSL* ssl, SSL_CTX * _ctx)
{
	VortexMutex * mutex;
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx   * ctx = vortex_connection_get_ctx (connection);
#endif

	/* set some necessary ssl data (setting destroy functions for
	 * both) */
	vortex_connection_set_data_full (connection, "ssl-data:ssl", ssl, 
					 NULL, (axlDestroyFunc) SSL_free);
	vortex_connection_set_data_full (connection, "ssl-data:ctx", _ctx, 
					 NULL, (axlDestroyFunc) SSL_CTX_free);

	/* set new handlers for read and write */
	vortex_log (VORTEX_LEVEL_DEBUG, "change default handlers to be used for send/recv");

	/* create the mutex */
	mutex = axl_new (VortexMutex, 1);
	vortex_mutex_create (mutex);
	
	/* configure the mutex used by the connection to protect the ssl session */	
	vortex_connection_set_data_full (connection, "ssl-data:mutex", mutex,
					 NULL, (axlDestroyFunc) __vortex_tls_free_mutex);
	vortex_connection_set_receive_handler (connection, vortex_tls_ssl_read);
	vortex_connection_set_send_handler    (connection, vortex_tls_ssl_write);

	return;
}

/** 
 * @internal
 * @brief Support data structure for \ref vortex_tls_start_negotiation function.
 */
typedef struct __VortexTlsBeginData {
	VortexConnection    * connection;
	const char          * serverName;
	VortexTlsActivation   process_status;
	axlPointer            user_data;
} VortexTlsBeginData;


void __vortex_tls_start_negotiation_close_and_notify (VortexTlsActivation   process_status,
						      VortexConnection    * connection,
						      VortexChannel       * channel,
						      char                * message,
						      axlPointer            user_data)
{
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx   * ctx = vortex_connection_get_ctx (connection);
#endif

	/* close the channel */
	if (channel != NULL)
		vortex_channel_close (channel, NULL);
	
	/* notify the error */
	if (process_status != NULL) {
		process_status (connection, VortexError, message, user_data);
	}

	/* log the error */
	vortex_log (VORTEX_LEVEL_CRITICAL, message, NULL);
	return;
}


/** 
 * @internal
 * @brief Invoke the specific TLS code to perform the handshake.
 * 
 * @param connection The connection where the TLS handshake will be performed.
 * 
 * @return true if the handshake was successfully performed. Otherwise
 * false is returned.
 */
bool     vortex_tls_invoke_tls_activation (VortexConnection * connection)
{
	/* get current context */
	VortexCtx            * ctx = vortex_connection_get_ctx (connection);
	SSL_CTX              * ssl_ctx;
	SSL_METHOD           * meth;
	SSL                  * ssl;
	X509                 * server_cert;
	VortexTlsCtxCreation   ctx_creation;
	axlPointer             ctx_creation_data;
	VortexTlsPostCheck     post_check;
	axlPointer             post_check_data;
	int                    ssl_error;
	VortexTlsCtx         * tls_ctx;

	/* check if the tls ctx was created */
	tls_ctx = vortex_ctx_get_data (ctx, TLS_CTX);
	if (tls_ctx == NULL) 
		return false;

	/* check if the connection have a ctx connection creation or
	 * the default ctx creation is configured */
	vortex_log (VORTEX_LEVEL_DEBUG, "initializing TLS context");
	ctx_creation      = vortex_connection_get_data (connection, CTX_CREATION);
	ctx_creation_data = vortex_connection_get_data (connection, CTX_CREATION_DATA);
	if (ctx_creation == NULL) {
		/* get the default ctx creation */
		ctx_creation      = tls_ctx->tls_default_ctx_creation;
		ctx_creation_data = tls_ctx->tls_default_ctx_creation_user_data;
	} /* end if */

	if (ctx_creation == NULL) {
		/* fall back into the default implementation */
		meth     = TLSv1_method ();
		ssl_ctx  = SSL_CTX_new (meth); 
	} else {
		/* call to the default handler to create the SSL_CTX */
		ssl_ctx  = ctx_creation (connection, ctx_creation_data);
	} /* end if */

	/* create and register the TLS method */
	if (ctx == NULL) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "error while creating TLS context");
		return false;
	}

	/* create the tls transport */
	vortex_log (VORTEX_LEVEL_DEBUG, "initializing TLS transport");
	ssl = SSL_new (ssl_ctx);       
	if (ssl == NULL) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "error while creating TLS transport");
		return false;
	}

	/* set the file descriptor */
	vortex_log (VORTEX_LEVEL_DEBUG, "setting file descriptor");
	SSL_set_fd (ssl, vortex_connection_get_socket (connection));

	/* configure read and write handlers and store default data to
	 * be used while sending and receiving data */
	vortex_tls_set_common_data (connection, ssl, ssl_ctx);

	/* do the initial connect connect */
	vortex_log (VORTEX_LEVEL_DEBUG, "connecting to remote TLS site");
	while (SSL_connect (ssl) <= 0) {
		
		vortex_log (VORTEX_LEVEL_WARNING, "accept function have failed (for initiator side)");
		ssl_error = SSL_get_error (ssl, -1);
		switch (ssl_error) {
		case SSL_ERROR_WANT_READ:
			vortex_log (VORTEX_LEVEL_WARNING, "still not prepared to continue because read wanted");
			break;
		case SSL_ERROR_WANT_WRITE:
			vortex_log (VORTEX_LEVEL_WARNING, "still not prepared to continue because write wanted");
			break;
		case SSL_ERROR_SYSCALL:
			vortex_log (VORTEX_LEVEL_CRITICAL, "syscall error while doing TLS handshake, ssl error (code:%d) : %s",
				    ssl_error, ERR_error_string (ssl_error, NULL));
			/* now the TLS process have failed because we
			 * are in the middle of a tuning process we
			 * have to close the connection because is not
			 * possible to recover previous BEEP state */
			__vortex_connection_set_not_connected (connection, "tls handshake failed");
			return false;
		default:
			vortex_log (VORTEX_LEVEL_CRITICAL, "there was an error with the TLS negotiation, ssl error (code:%d) : %s",
				    ssl_error, ERR_error_string (ssl_error, NULL));
			/* now the TLS process have failed, we have to
			 * restore how is read and written the data */
			vortex_connection_set_default_io_handler (connection);
			return false;
		}
	}
	vortex_log (VORTEX_LEVEL_DEBUG, "seems SSL connect call have finished in a proper manner");

	/* check remote certificate (if it is present) */
	server_cert = SSL_get_peer_certificate (ssl);
	if (server_cert == NULL) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "server side didn't set a certificate for this session, this is a bad news");
		vortex_support_free (2, ssl, SSL_free, ctx, SSL_CTX_free);
		return false;
	}
	X509_free (server_cert);

	/* post SSL activation checkings */
	post_check      = vortex_connection_get_data (connection, POST_CHECK);
	post_check_data = vortex_connection_get_data (connection, POST_CHECK_DATA);
	if (post_check == NULL) {
		/* get the default ctx creation */
		post_check      = tls_ctx->tls_default_post_check;
		post_check_data = tls_ctx->tls_default_post_check_user_data;
	} /* end if */
	
	if (post_check != NULL) {
		/* post check function found, call it */
		if (! post_check (connection, post_check_data, ssl, ssl_ctx)) {
			/* found that the connection didn't pass post checks */
			__vortex_connection_set_not_connected (connection, "post checks failed");
			return false;
		} /* end if */
	} /* end if */

	vortex_log (VORTEX_LEVEL_DEBUG, "TLS transport negotiation finished");
	
	return true;
}

/** 
 * @internal
 *
 * @brief Support function for \ref vortex_tls_start_negotiation which actually do
 * the work for that function without knowing if it is running as a
 * threaded mode or not.
 * 
 * @param data \ref VortexTlsBeginData having all data need to
 * negotiate the TLS process.
 * 
 * @return Always a NULL value.
 */
axlPointer __vortex_tls_start_negotiation (VortexTlsBeginData * data)
{
	VortexChannel         * channel         = NULL;
	VortexConnection      * connection      = data->connection;
	VortexConnection      * connection_aux  = NULL;
	VortexCtx             * ctx             = vortex_connection_get_ctx (connection);
	const char            * serverName      = data->serverName;
	VortexTlsActivation     process_status  = data->process_status;
	axlPointer              user_data       = data->user_data;
	VortexFrame           * reply           = NULL;
	VortexAsyncQueue      * queue;
	VORTEX_SOCKET           socket;
	bool                    status          = false;

	/* free no longer needed data  */
	axl_free (data);

	/* Send TLS <ready> message. This will signal the remote peer
	* we need to negotiate a TLS secure layer, once the remote
	* site receive such petition, it should reply using <proceed>
	* or <error>.
	*
	* Actually send a <ready> message means to create a channel
	* using TLS profile piggybacking an initial data, that is,
	* the <ready /> token. 
	* 
	* Piggybacking is just a way to save a round trip based on
	* creating the channel based on TLS profile and then send a
	* MSG with a <ready /> token.  We send with the same message,
	* the channel creation request and the first message sent. If
	* the channel is accepted to be created, the profile content,
	* that is, the piggyback, will be accepted as the first
	* message. */
	vortex_log (VORTEX_LEVEL_DEBUG, "TLS STAGE[1]: create the TLS channel");
	queue   = vortex_async_queue_new ();
	channel = vortex_channel_new_full (connection, /* the connection */
					   0,          /* the channel vortex chose */
					   serverName, /* the serverName value (no matter if it is NULL) */
					   /* the TLS profile identifier */
					   VORTEX_TLS_PROFILE_URI,
					   /* content encoding */
					   EncodingNone,
					   /* initial content or piggyback and its size */
					   "<ready />", 9,
					   /* close channel notification: we don't set it. */
					   NULL, NULL,
					   /* frame received notification: we don't set it. */
					   vortex_channel_queue_reply, queue,
					   /* on channel crated notification: we don't set it.
					    * It is not needed, we are working on a separated
					    * thread. */
					   NULL, NULL);

	/* As you may observed, previous code doesn't define a close
	 * handler. This is because once the channel is accepted to be
	 * created the TLS negotiation will start and no BEEP message
	 * will be exchanged until the TLS negotiation have finished,
	 * and all channels opened will be closed, including the
	 * channel 0.
	 * 
	 * Once terminated the TLS negotiation, no matter its result,
	 * a new channel 0 will be created, sending again the initial
	 * greeting.
	 *
	 * check channel status */
	if (channel == NULL) {
		/* unref queue */
		vortex_async_queue_unref (queue);

		/* notify user app level */
		__vortex_tls_start_negotiation_close_and_notify (process_status,
								 connection,
								 channel,
								 "Timeout reached while waiting for initial reply to TLS profile query",
								 user_data);
		return NULL;
	}
	vortex_log (VORTEX_LEVEL_DEBUG, "TLS STAGE[2]: TLS channel accepted, waiting for reply (as a piggyback or a plain message)");

	/* 2. wait for receive the proceed or error message:  
	 *
	 * Once the channel is started, we have to receive a reply
	 * message having a <error /> or <proceed />. That reply could
	 * come as a piggyback reply or a simple message. Both
	 * functionality is supported by the \ref
	 * vortex_channel_get_reply function. */

	reply = vortex_channel_get_reply (channel, queue);

	/* unref queue */
	vortex_async_queue_unref (queue);

	vortex_log (VORTEX_LEVEL_DEBUG, "TLS STAGE[3]: TLS reply received");
	if (reply == NULL) {
		/* no reply received, within the timeout configured or
		 * remote peer have a poorly TLS implementation */
		__vortex_tls_start_negotiation_close_and_notify (process_status,
								 connection,
								 channel,
								 "Timeout reached while waiting for initial reply to TLS profile query",
								 user_data);
		return NULL;
	}

	/* In case of a error message is received, notify user space
	 * app and return */
	vortex_log (VORTEX_LEVEL_DEBUG, "TLS STAGE[4]: check TLS reply received ");
	switch (vortex_frame_get_type (reply)) {
	case VORTEX_FRAME_TYPE_RPY:
		if (!axl_cmp (vortex_frame_get_payload (reply), "<proceed />")) {
			/* content received */
			vortex_log (VORTEX_LEVEL_CRITICAL, 
			       "Wrong reply received from remote peer for TLS support (<proceed /> != '%s'",
			       vortex_frame_get_payload (reply));

			/* received a unexpected token reply */
			vortex_frame_unref (reply);
			__vortex_tls_start_negotiation_close_and_notify (process_status,
									 connection,
									 channel, 
									 "Wrong reply received from remote peer for TLS support",
									 user_data);
			return NULL;
		} /* end if */

		break;
	case VORTEX_FRAME_TYPE_ERR:
		/* received a unexpected token reply */
		vortex_frame_unref (reply);
		__vortex_tls_start_negotiation_close_and_notify (process_status,
								 connection,
								 channel, 
								 "TLS activations was rejected", user_data);
		return NULL;
	default:
		/* unexpected reply received */
		vortex_frame_unref (reply);
		__vortex_tls_start_negotiation_close_and_notify (process_status,
								 connection,
								 channel, "Unexpected frame type received", 
								 user_data);
		return NULL;
	}
	/* free reply value */
	vortex_frame_unref (reply);

	vortex_log (VORTEX_LEVEL_DEBUG, "TLS STAGE[5]: everything is fine, close all channels..");


	/* prevent vortex library to close the underlying socket once
	 * the connection is closed. */
	vortex_connection_set_close_socket (connection, false);
	
	/* get the socket where the underlying transport negotiation
	 * is taking place. */
	socket = vortex_connection_get_socket (connection);
	vortex_log (VORTEX_LEVEL_DEBUG, "TLS STAGE[6]: channels closed, start tls negotiation on socket=%d..", socket);

	/* get a reference to previous connection */
	connection_aux = connection;

	/* create a new connection object, set the socket, using the
	 * same connection reference. This is could be seen as not
	 * necessary but it is.  The \ref VortexConnection object have
	 * functions to to define what to do when the connection is
	 * closed, how the data is send and received, or some functions
	 * to store and retrieve data.  */
	connection = vortex_connection_new_empty_from_connection (ctx, socket, connection_aux, VortexRoleInitiator);

	/* unref the vortex connection and create a new empty one */
	__vortex_connection_set_not_connected (connection_aux, 
					       "connection instance being closed, without closing session, due to underlying TLS negotiation");
	/* dealloc the connection */
	vortex_connection_unref (connection_aux, "(vortex tls process)");
	
	/* the the connection to be blocking during the TLS
	 * negotiation, once called
	 * vortex_connection_parse_greetings_and_enable, the
	 * connection will be again non-blocking. */
	vortex_connection_set_blocking_socket (connection);

	/* do the TLS voodoo */
	status = vortex_tls_invoke_tls_activation (connection);

	vortex_log (status ? VORTEX_LEVEL_DEBUG : VORTEX_LEVEL_CRITICAL, 
	       "TLS STAGE[6.1] TLS negotiation status is: %s", status ? "OK" : "*** FAIL ***");
	
	vortex_log (VORTEX_LEVEL_DEBUG, "TLS STAGE[7]: TLS voodoo have finished, the connection is created and registered into the vortex writer, sending greetings..");
	/* check wrong status after TLS negotiation */
	if (! status) {
		/* notify that the TLS negotiation have failed
		 * because the initial greetings failure */
		__vortex_tls_start_negotiation_close_and_notify (process_status,
								 connection,
								 NULL, 
								 "TLS negotiation has failed, unable to procede with BEEP session tuning.", user_data);
		return NULL;
	}


	/* 5. Issue again initial greetings, greetings just like we
	 * were creating a connection. */
	if (!vortex_greetings_client_send (connection)) {
                vortex_log (VORTEX_LEVEL_CRITICAL, "failed while sending greetings after TLS negotiation");

		/* notify that the TLS negotiation have failed
		 * because the initial greetings failure */
		__vortex_tls_start_negotiation_close_and_notify (process_status,
								 connection,
								 NULL, 
								 "Initial greetings before a TLS negotiation has failed", user_data);
		return NULL;
	}

	vortex_log (VORTEX_LEVEL_DEBUG, "TLS STAGE[8]: greetings sent, waiting for reply");

	/* 6. Wait to get greetings reply (again issued because the
	 * underlying transport reset the session state) */
	reply = vortex_greetings_client_process (connection);
	
	vortex_log (VORTEX_LEVEL_DEBUG, "TLS STAGE[9]: reply received, checking content");

	/* 7. Check received greetings reply, read again supported
	 * profiles from remote site and register the connection into
	 * the vortex reader to start exchanging data. */
	if (!vortex_connection_parse_greetings_and_enable (connection, reply)) {

		/* notify the user the sad new! */
		__vortex_tls_start_negotiation_close_and_notify (process_status,
								 connection,
								 NULL,
								 "Greetings response parsing have failed before successful TLS negotiation", 
								 user_data);
		return NULL;
	}

	vortex_log (VORTEX_LEVEL_DEBUG, "TLS STAGE[10]: greetings reply received is ok, connection is ready and registered into the vortex reader (that's all)");

	/* flag this connection to be already TLS-ficated if the
	 * status is ok */
	if (status) {
		vortex_connection_set_tlsfication_status (connection, true);
	}

	/* 8. finally, report current TLS negotiation status to the
	 * app level.  notify the error */
	if (process_status != NULL) {
		/* notify user space */
		process_status (connection, 
				(status) ? VortexOk : VortexError, 
				(status) ? "TLS negotiation finished, now we can talk using the ciphered way!" :
				"TLS negotiation have failed!",  user_data);
	}
	return NULL;
}

/** 
 * @brief Starts the TLS underlying transport security negotiation
 * for the given connection.
 *
 * This function implements the client side for the TLS
 * profile. Inside the BEEP protocol definition, the TLS profile
 * provides a way to secure peer to peer data transmission, allowing
 * to take advantage of it from any profile implemented on top of the
 * BEEP stack.  TLS profile allows to secure underlying network
 * connection using the TLS v1.0 protocol.
 *
 * Here is an example on how to activate the TLS profile (details are explained after the example):
 * 
 * \code
 * // simple function with triggers the TLS process
 * void activate_tls (VortexCtx * ctx, VortexConnection * connection) 
 * {
 *     // initialize and check if current vortex library supports TLS
 *     if (! vortex_tls_init (ctx)) {
 *         printf ("Unable to activate TLS, Vortex is not prepared\n");
 *         return;
 *     }
 *
 *     // start the TLS profile negotiation process
 *     vortex_tls_start_negotiation (ctx, connection, NULL, 
 *                                   process_tls_status, NULL);
 * }
 *
 * // simple function with process the TLS status
 * void process_tls_status (VortexConnection * connection,
 *                          VortexStatus       status,
 *                          char             * status_message,
 *                          axlPointer         user_data)
 * {
 *      switch (status) {
 *      case VortexOk:
 *           printf ("TLS negotiation OK! over the new connection %d\n",
 *                    vortex_connection_get_id (connection));
 *
 *           // use the new connection reference provided by 
 *           // this function. Connection provided at 
 *           // vortex_tls_start_negotiation have been unrefered.
 *           // In this case, my_server_connection have been 
 *           // already unrefered by the TLS activation
 *           my_server_connection = connection;           
 *
 *           break;
 *      case VortexError: 
 *           printf ("TLS negotiation have failed, message: %s\n",
 *                    status_message);
 *           // ok, TLS process have failed but, do we have a connection
 *           // still working?. This is checked using
 *           if (vortex_connection_is_ok (connection, false)) {
 *              // well we don't have TLS activated but the connection
 *              // still works
 *              my_server_connection = connection;
 *           } 
 *           break;
 *      }
 *      return;
 * }
 * \endcode
 * 
 * As the previous example shows, the function requires the connection
 * where the TLS process will take place, a handler to \ref VortexTlsActivation "notify the TLS status",
 * an optional serverName string to notify listener side to act as server name and an optional user data.
 *
 * While the TLS negotiation is running, user application could still
 * perform other tasks because this function doesn't block the
 * caller. Once the TLS process have finished, no matter its result,
 * the status will be notified using \ref VortexTlsActivation "process_status" handler.
 *
 * Because the TLS negotiation involves a tuning reset, the
 * connection provided to this function will be unrefered without
 * closing the underlying socket. This means that the connection
 * provided at this function must be no longer used, closed or
 * unrefered. 
 * 
 * The connection provided at \ref VortexTlsActivation
 * "process_status" handler is the one to be used once the TLS process
 * have finished properly.
 *
 * Because the TLS negotiation could fail user code placed at the
 * process function should check if the new connection provided on
 * that function is still working. This is because many things could
 * fail while TLS negotiation takes place. Some of them represents
 * protocol violation involving the connection to be closed, the
 * others just means the TLS have failed but the connection is still
 * valid not only to try again the TLS activation but to use it
 * without secure transmission.
 *
 * Once a connection is TLS-ficated the function just return without
 * doing nothing, if an attempt to secure the connection is made. You
 * could check TLS status for a given connection using \ref
 * vortex_connection_is_tlsficated.
 *
 * A call to \ref vortex_tls_init is required to start the TLS context
 * inside the provided \ref VortexCtx. Next calls won't perform any
 * operation than returning true (signaling the library is
 * initialized).
 *
 * If you need to know more about how to activate TLS support on
 * server side (well, actually for the listener role) check out this
 * function: \ref vortex_tls_accept_negotiation.
 * 
 * Additionally, you can also consider to make Vortex Library to
 * automatically handle TLS profile negotiation for every connection
 * created. This is done by using \ref vortex_tls_set_auto_tls
 * function. Once activated, every call to \ref vortex_connection_new
 * will automatically negotiate the TLS profile once connected before
 * notifying user space code for the connection creation.  *
 *
 * <b>NOTE:</b> As part of the TLS negotiation, all channels inside
 * the given connection will be closed.
 *
 * <b>NOTE:</b> The connection provided will be unrefered and a new
 * connection will be provided by this function, but using the same
 * underlying transport descriptor. This means the connection passed
 * to this function and the connection created by this function will
 * both link to the same peer but being different instances.
 * 
 * @param connection The connection where the secure underlying transport
 * will be started.
 *
 * @param serverName A server name value to be notified to the
 * remote peer so it could react in a different way depending on this
 * value. Function will perform a copy from the given value if it is
 * not a NULL one. You can free the passed in value just after the
 * function returns.
 *
 * @param process_status A handler to be executed once the process
 * have finished, no matter its result. This handler is required.
 *
 * @param user_data A user defined data to be passed in to the
 * <i>process_status</i> handler.
 *
 *
 */
void vortex_tls_start_negotiation (VortexConnection     * connection,
				   const char           * serverName,
				   VortexTlsActivation    process_status,
				   axlPointer             user_data)
{
	/* TLS supported case */
	VortexTlsBeginData * data;
	VortexCtx          * ctx = vortex_connection_get_ctx (connection);

	/* check environment conditions */
	v_return_if_fail (connection);
	v_return_if_fail (vortex_connection_is_ok (connection, false));
	v_return_if_fail (process_status);


	/* check for already TLS-fication */
	if (vortex_connection_is_tlsficated (connection)) {
		vortex_log (VORTEX_LEVEL_WARNING, "Trying to TLS-ficate a connection already running TLS profile");
		if (process_status != NULL) {
			process_status (connection, VortexOk, 
					"Trying to TLS-ficate a connection already running TLS profile",
					user_data);
			return;
		} 
		/* no handler defined */
		vortex_log (VORTEX_LEVEL_CRITICAL, "no process status found, you should define this handler.");
		return;
	}


	/* check if remote site support remote profile (this step is
	 * actually not needed because vortex_channel_new already
	 * supports checking for remote profile to be supported. */
	if (!vortex_connection_is_profile_supported (connection, VORTEX_TLS_PROFILE_URI)) {
		vortex_log (VORTEX_LEVEL_WARNING, "remote peer doesn't support TLS profile, unable to activate TLS support");
		if (process_status != NULL) {
			process_status (connection, VortexError, 
					"Remote peer doesn't support TLS profile. Unable to negotiate secure transport layer",
					user_data);
			return;
		} 

		/* no handler defined */
		vortex_log (VORTEX_LEVEL_CRITICAL, "no process status found, you should define this handler.");
		return;
	}

	/* create and prepare invocation */
	data                 = axl_new (VortexTlsBeginData, 1);
	data->connection     = connection;
	data->serverName     = (serverName != NULL) ? axl_strdup (serverName) : serverName;
	data->process_status = process_status;
	data->user_data      = user_data;

	vortex_thread_pool_new_task (ctx, (VortexThreadFunc) __vortex_tls_start_negotiation, data);
	return;
}

/**
 * @internal Mask installed to ensure that TLS profile isn't reused
 * again for the particular connection.
 */
bool __vortex_tls_mask (VortexConnection  * connection,
			int                 channel_num,
			const char        * uri,
			const char        * profile_content,
			const char        * serverName,
			char             ** error_msg,
			axlPointer         user_data)
{
	/* if both strings are equal, filter the profile */
	return axl_cmp (uri, VORTEX_TLS_PROFILE_URI);
} /* end if */


/** 
 * @internal
 *
 * @brief Perform final operations to accept the TLS incoming
 * negotiation.
 * 
 * @param connection The connection to be finally accepted.
 */
void vortex_tls_initial_accept (VortexConnection * connection)
{
	/* get current context */
	VortexCtx            * ctx       = vortex_connection_get_ctx (connection);
	SSL                  * ssl       = vortex_connection_get_data (connection, "ssl-data:ssl");
	SSL_CTX              * ssl_ctx   = vortex_connection_get_data (connection, "ssl-data:ctx");
	bool                   status    = false;
	VortexTlsPostCheck     post_check;
	axlPointer             post_check_data;
	VortexTlsCtx         * tls_ctx;

	/* check if the tls ctx was created */
	tls_ctx = vortex_ctx_get_data (ctx, TLS_CTX);
	if (tls_ctx == NULL) 
		return;

	/* accept the incoming connection */
	if (SSL_accept (ssl) == -1) {
		vortex_log (VORTEX_LEVEL_WARNING, "accept function have failed (for listener side)");
		switch (SSL_get_error (ssl, -1)) {
		case SSL_ERROR_WANT_READ:
			vortex_log (VORTEX_LEVEL_WARNING, "still not prepared to continue because read wanted");
			return;
		case SSL_ERROR_WANT_WRITE:
			vortex_log (VORTEX_LEVEL_WARNING, "still not prepared to continue because write wanted");
			return;
		default:
			/* TLS-fication process have failed */
			vortex_log (VORTEX_LEVEL_CRITICAL, "there was an error while accepting TLS connection");
			
			/* restor default IO handlers  */
			vortex_connection_set_default_io_handler (connection);
		}
	}else {
		/* TLS-fication done */
		status = true;
	}
	vortex_log (VORTEX_LEVEL_DEBUG, "TLS-fication status was: %s", status ? "OK" : "*** FAIL ***");

	/* Disable preread handler for the connection. We have finished */
	vortex_connection_set_preread_handler (connection, NULL);

	if (status) {

		/* check for post-condition checks to be done by the
		 * application level */

		/* post SSL activation checkings */
		post_check      = vortex_connection_get_data (connection, POST_CHECK);
		post_check_data = vortex_connection_get_data (connection, POST_CHECK_DATA);
		if (post_check == NULL) {
			/* get the default ctx creation */
			post_check      = tls_ctx->tls_default_post_check;
			post_check_data = tls_ctx->tls_default_post_check_user_data;
		} /* end if */
		
		if (post_check != NULL) {
			/* post check function found, call it */
			if (! post_check (connection, post_check_data, ssl, ssl_ctx)) {
				/* found that the connection didn't pass post checks */
				__vortex_connection_set_not_connected (connection, "post checks failed");
				return;
			} /* end if */
		} /* end if */
		
		/* flag this connection to be already TLS-ficated */
		vortex_connection_set_tlsfication_status (connection, true);

		/* install here a filtering mask to avoid reusing tls
		 * profile again inside the greetings */
		vortex_connection_set_profile_mask (connection, __vortex_tls_mask, NULL);
		
	} /* end if */

	/* 5. Issue again initial greetings, greetings just like we
	 * were creating a connection. */
	if (!vortex_greetings_send (connection)) {
		/* because the greeting connection have failed, we
		 * have to close and drop the \ref VortexConnection
		 * object. */
		vortex_connection_unref (connection, "(vortex tls profile)");
		
	}

	return;
}


/** 
 * @internal
 * @brief Prepares the connection for the TLS profile.
 * 
 * This function is executed when the on close event have happen. This
 * means the all channels are closed and we can start the TLS handshake.
 * 
 * @param connection The connection being closed.
 */
void vortex_tls_prepare_listener (VortexConnection * connection)
{
	/* get current context */
	VortexCtx            * ctx = vortex_connection_get_ctx (connection);
	SSL_METHOD           * meth;
	SSL_CTX              * ssl_ctx;
	SSL                  * ssl;
	char                 * certificate_file;
	char                 * private_file;
	VortexConnection     * new_connection;
	VORTEX_SOCKET          socket;
	VortexTlsCtxCreation   ctx_creation;
	axlPointer             ctx_creation_data;
	VortexTlsCtx         * tls_ctx;

	/* check if the tls ctx was created */
	tls_ctx = vortex_ctx_get_data (ctx, TLS_CTX);
	if (tls_ctx == NULL) 
		return;

	vortex_log (VORTEX_LEVEL_DEBUG, "State[1] session is being closed this means we can start negotiating TLS");
	socket = vortex_connection_get_socket (connection);

	/* check if the connection have a ctx creation method or the
	 * default one is activated. */
	ctx_creation      = vortex_connection_get_data (connection, CTX_CREATION);
	ctx_creation_data = vortex_connection_get_data (connection, CTX_CREATION_DATA);
	if (ctx_creation == NULL) {
		/* get the default ctx creation */
		ctx_creation      = tls_ctx->tls_default_ctx_creation;
		ctx_creation_data = tls_ctx->tls_default_ctx_creation_user_data;
	} /* end if */

	if (ctx_creation == NULL) {
		meth     = TLSv1_method ();
		ssl_ctx  = SSL_CTX_new (meth);
	} else {
		/* call to the default handler to create the SSL_CTX */
		ssl_ctx  = ctx_creation (connection, ctx_creation_data);
	} /* end if */
	
	if (ssl_ctx == NULL) {
		/* drop a log and instruct vortex library to close the
		 * given connection including the socket. */
		vortex_log (VORTEX_LEVEL_CRITICAL, 
		       "unable to create SSL context object, unable to start TLS profile");
		vortex_connection_set_close_socket (connection, true);
		vortex_connection_shutdown (connection);
		return;
	} /* end if */
	

	/* if the context creation is provided, do not perform the
	 * following tasks */
	if (ctx_creation == NULL) {

		/* configure certificate file */
		certificate_file = vortex_connection_get_data (connection, "tls:certificate-file");
		printf ("Using certificate: %s\n", certificate_file);
		if (SSL_CTX_use_certificate_file (ssl_ctx, certificate_file, SSL_FILETYPE_PEM) <= 0) {
			vortex_log (VORTEX_LEVEL_CRITICAL, 
				    "there was an error while setting certificate file into the SSl context, unable to start TLS profile");
			vortex_connection_set_close_socket (connection, true);
			vortex_connection_shutdown (connection);
			return;
		}
		
		/* configure private file */
		private_file    = vortex_connection_get_data (connection, "tls:private-file");
		if (SSL_CTX_use_PrivateKey_file (ssl_ctx, private_file, SSL_FILETYPE_PEM) <= 0) {
			vortex_log (VORTEX_LEVEL_CRITICAL, 
				    "there was an error while setting private file into the SSl context, unable to start TLS profile");
			vortex_connection_set_close_socket (connection, true);
			vortex_connection_shutdown (connection);
			return;
		}

		/* check for private key and certificate file to match. */
		if (!SSL_CTX_check_private_key(ssl_ctx)) {
			vortex_log (VORTEX_LEVEL_CRITICAL, 
				    "seems that certificate file and private key doesn't match!, unable to start TLS profile");
			vortex_connection_set_close_socket (connection, true);
			vortex_connection_shutdown (connection);
			return;
		} /* end if */

	} /* end if */
		
	/* create ssl object */
	vortex_log (VORTEX_LEVEL_DEBUG, "initializing TLS transport");
	ssl = SSL_new (ssl_ctx);       
	if (ssl == NULL) {
		vortex_log (VORTEX_LEVEL_CRITICAL, "error while creating TLS transport");
		vortex_connection_set_close_socket (connection, true);
		vortex_connection_shutdown (connection);
		return;
	}

	/* set the file descriptor */
	vortex_log (VORTEX_LEVEL_DEBUG, "setting file descriptor");
	SSL_set_fd (ssl, socket);

	/* prepare the new connection */
	new_connection = vortex_connection_new_empty_from_connection (ctx, socket, connection, VortexRoleListener);

	/* release previous objet */
	__vortex_connection_set_not_connected (connection, 
					       "connection instance being closed, without closing session, due to underlaying TLS negoctiation");


	/* set default handlers to write/read and default objects to
	 * be used */
	vortex_tls_set_common_data (new_connection, ssl, ssl_ctx);

	/* set the next preread handler, where the tls connection will
	 * be finally accepted */
	vortex_connection_set_preread_handler (new_connection, vortex_tls_initial_accept);

	/* accept the connection in an initial step (actually not
	 * executed until the pre-read handler is not executed). This
	 * function flag the connection to be on the initial stage and
	 * register the connection into the vortex reader.
	 *
	 * The second parameter means that we don't want to send the
	 * greetings at this point. First, we have to ensure the TLS
	 * layer is sucessfully activated to be able to perform a
	 * proper greetings advice. 
	 * 
	 * We'll do it at the pre read handler
	 * (vortex_tls_initial_accept) once the TLS layer is properly
	 * working. */
	vortex_listener_accept_connection (new_connection, false);

	return;
}

/** 
 * @internal
 *
 * @brief Vortex TLS start handler which process and notify user space
 * all start TLS channel queries.
 * 
 * 
 * 
 * @param profile 
 * @param channel_num 
 * @param connection 
 * @param serverName 
 * @param profile_content 
 * @param profile_content_reply
 * @param encoding 
 */
bool     vortex_tls_process_start_msg (char              * profile,
				       int                 channel_num,
				       VortexConnection  * connection,
				       char              * serverName,
				       char              * profile_content,
				       char             ** profile_content_reply,
				       VortexEncoding      encoding,
				       axlPointer          user_data)
{
	/* get current context */
	VortexCtx    * ctx = vortex_connection_get_ctx (connection);
	char         * certificate_file;
	char         * private_key_file;
	VortexTlsCtx * tls_ctx;

	/* check if the tls ctx was created */
	tls_ctx = vortex_ctx_get_data (ctx, TLS_CTX);
	if (tls_ctx == NULL) 
		return false;

	/* flag this connection to be already TLS-ficated */
	if (vortex_connection_is_tlsficated (connection)) {
		vortex_log (VORTEX_LEVEL_WARNING, "received a TLS channel request on an already TLS-ficated connection.");
		return false;
	}

	/* check profile content */
	vortex_log (VORTEX_LEVEL_DEBUG, "received TLS negotiation request");
	if (profile_content == NULL || !axl_cmp (profile_content, "<ready />")) {
		vortex_log (VORTEX_LEVEL_WARNING, "received a TLS channel request having a missing or wrong profile content");
		return false;
	}
	
	/* notify user app level that a TLS profile request have being
	 * received and it is required to act as serverName. */
	vortex_log (VORTEX_LEVEL_DEBUG, "checking if application level allows to negotiate the TLS profile");
	if (! tls_ctx->tls_accept_handler (connection, serverName)) {
		(* profile_content_reply)  = vortex_frame_get_error_message ("421", "application level have reject to negotiate TLS transport layer", NULL);
		
		/* we have to reply true because the expected reply is
		 * a positive one with a proceed or error content. */
		return true;
	}

	if (tls_ctx->tls_default_ctx_creation == NULL && 
	    vortex_connection_get_data (connection, CTX_CREATION) == NULL) {
		
		vortex_log (VORTEX_LEVEL_DEBUG, "application level seems to accept negotiate the TLS profile, getting certificate");

		/* get TLS certificate file */
		certificate_file = tls_ctx->tls_certificate_handler (connection, serverName);
		if (certificate_file == NULL) {
			(* profile_content_reply)  = 
				vortex_frame_get_error_message ("421", 
								"application level didn't provide a valid location for a certificate file, unable to negotiate TLS transport", 
								NULL);
			/* we have to reply true because the expected reply is
			 * a positive one with a proceed or error content. */
			return true;
		}
		
		vortex_log (VORTEX_LEVEL_DEBUG, "getting private key file");
		
		/* get TLS private key file */
		private_key_file = tls_ctx->tls_private_key_handler (connection, serverName);
		if (private_key_file == NULL) {
			(* profile_content_reply)  = vortex_frame_get_error_message ("421", 
										     "application level didn't provide a valid location for a private key file, unable to negotiate TLS transport", 
										     NULL);
			/* we have to reply true because the expected reply is
			 * a positive one with a proceed or error content. */
			return true;
		}

		/* store certificate and private key files */
		vortex_connection_set_data_full    (connection, "tls:certificate-file", certificate_file, NULL, axl_free);
		vortex_connection_set_data_full    (connection, "tls:private-file",     private_key_file, NULL, axl_free);
	}
		
	/* set the reply to for the TLS channel negotiation. Memory
	 * holding profile content reply should be dynamically
	 * allocated because vortex library will deallocate it. */
	(* profile_content_reply)  = axl_strdup ("<proceed />");
	vortex_log (VORTEX_LEVEL_DEBUG, "replying peer that TLS negotiation can start");
	
	/* Here goes the trick that makes tunning reset at the server
	 * side to be possible.
	 * 
	 * we prepare the socket connection to not be closed even if
	 * the channel 0 is closed on the given connection. */
	vortex_connection_set_close_socket (connection, false);


	/* now prepare the connection to accept the incoming
	 * negotiation by using the pre read handler */
	vortex_connection_set_preread_handler (connection, vortex_tls_prepare_listener);

	return true;
}


/** 
 * @internal
 * This default hander definition is used when no accept handler was
 * defined. It always returns true, which means to always accept the
 * TLS request.
 */
bool     vortex_tls_default_accept     (VortexConnection * connection,
				        char             * serverName)
{
	return true;
}

/** 
 * @internal
 * @brief Default certificate locator function which returns the test
 * certificate used by the Vortex Library.
 */
char  * vortex_tls_default_certificate (VortexConnection * connection,
				        char             * serverName)
{
	VortexCtx   * ctx         = vortex_connection_get_ctx (connection);
	char        * certificate;

	certificate = vortex_support_find_data_file (ctx, "test-certificate.pem");

	vortex_log (VORTEX_LEVEL_DEBUG, "getting default test certificate: %s (ctx: %p)", certificate, ctx);
	
	/* return certificate found */
	return certificate;
}

/** 
 * @internal
 *
 * @brief Default private key locator function which returns the test
 * private key used by the Vortex Library.
 */
char  * vortex_tls_default_private_key (VortexConnection * connection,
				        char             * serverName)
{
	VortexCtx   * ctx = vortex_connection_get_ctx (connection);

	return vortex_support_find_data_file (ctx, "test-private-key.pem");
}


typedef struct _VortexTlsSyncResult {
	VortexConnection * connection;
	VortexStatus       status;
	char             * status_message;
} VortexTlsSyncResult;

/** 
 * @internal
 *
 * @brief Support function for \ref vortex_tls_start_negotiation_sync
 * function.
 */
void __vortex_tls_start_negotiation_sync_process (VortexConnection * connection,
						  VortexStatus       status,
						  char             * status_message,
						  axlPointer         user_data)
{
	VortexAsyncQueue    * queue = user_data;
#if defined(ENABLE_VORTEX_LOG)
	VortexCtx           * ctx = vortex_connection_get_ctx (connection);
#endif
	VortexTlsSyncResult * result;

	/* create a structure to hold all the result produced. */
	result                 = axl_new (VortexTlsSyncResult, 1);
	result->connection     = connection;
	result->status         = status;
	result->status_message = status_message;

	/* push and unref man! */
	QUEUE_PUSH  (queue, result);
	vortex_async_queue_unref (queue);
	
	return;
}



/** 
 * @brief Allows to start a TLS profile negotiation in a synchronous
 * way (blocking the caller).
 *
 * This actually uses \ref vortex_tls_start_negotiation. Check out
 * \ref vortex_tls_start_negotiation documentation to know more about
 * activating TLS profile.
 * 
 * If TLS profile is not supported the function returns the same
 * connection received. A call to \ref vortex_tls_init is required
 * before calling to this function. 
 *
 * @param connection The connection where the TLS profile negotiation will take place.
 * @param serverName The serverName optional value to request remote peer to act as server name.
 * @param status     An optional reference to a \ref VortexStatus variable so the caller could get a TLS activation status.
 * @param status_message An optional reference to get a textual diagnostic about TLS activation status.
 * 
 * @return The new connection with TLS profile activated. 
 * 
 * <i><b>NOTE: About connection reference returned</b><br> 
 * 
 * The function always return a reference to a connection, even on TLS
 * failures. Assuming this, it is required to call to \ref
 * vortex_connection_is_ok to check status and proceed as requried if
 * the case an error was found. The same can be done using
 * <i>status</i> or <i>status_message</i> parameters (checking state).
 * 
 * Because the function could fail before the TLS handshake can start
 * or during it, the function returns the same connection if the
 * failure was found before TLS started or a different connection
 * reference (with the same data) if the error is found during the
 * tuning. In both cases a valid reference (which is not the same as
 * valid connection) is always returned (even if the connection is not
 * working).
 * 
 * </i>
 */
VortexConnection * vortex_tls_start_negotiation_sync     (VortexConnection  * connection,
							  const char        * serverName,
							  VortexStatus      * status,
							  char             ** status_message)
{
	/* new connection received */
	VortexCtx           * ctx = vortex_connection_get_ctx (connection);
	VortexConnection    * _connection;
	VortexTlsSyncResult * result;
	VortexAsyncQueue    * queue;

	/* create an async queue and increase queue reference so the
	 * function could unref it without worry about race conditions
	 * with sync_process function. */
	queue = vortex_async_queue_new ();
	vortex_async_queue_ref (queue);

	/* start TLS negotiation */
	vortex_tls_start_negotiation (connection, serverName, 
				      __vortex_tls_start_negotiation_sync_process,
				      queue);

	/* get status */
	result = vortex_async_queue_timedpop (queue, vortex_connection_get_timeout (ctx));
	if (result == NULL) {
		/* seems timeout have happen while waiting for SASL to
		 * end */
		if (status != NULL)
			(* status)         = VortexError;
		if (status_message != NULL)
			(* status_message) = "Timeout have been reached while waiting for TLS to finish";

		/* return the same connection */
		return connection;
	}

	/* get status */
	if (status != NULL)
		(* status) = result->status;
	
	/* get message */
	if (status_message != NULL)
		(* status_message) = result->status_message;

	/* get connection */
	_connection = result->connection;

	/* free and unref the queue */
	axl_free (result);
	vortex_async_queue_unref (queue);

	/* return received value */
	return _connection;
}


/** 
 * @brief Allows to configure if current Vortex Library instance will
 * accept TLS incoming connections.
 * 
 * While using TLS under BEEP, as a peer protocol, any instance
 * running could receive a TLS request for its activation. This
 * function allows to configure if such request will be allowed or
 * recognized. 
 * 
 * Default TLS configuration is to not allow receive a TLS request. 
 *
 * This function doesn't disable the possibility to connect to a remote
 * peer and request to negotiate the TLS security transport. 
 *
 * There are two typical scenarios:
 *
 * - 1. A Vortex Library client peer do not to call this
 * function. However, once required to activate the TLS profile, the
 * client peer could issue a call to \ref vortex_tls_start_negotiation
 * to enable TLS transport against a BEEP peer accepting TLS
 * requests. In the case the remote peer is running Vortex Library,
 * that peer must have already issued a call to \ref
 * vortex_tls_accept_negotiation to accept the incoming TLS request.
 * 
 * - 2. In the other hand, Vortex Library listeners could enable
 * accepting TLS incoming connections so they publish as a possible
 * profile the TLS one. This is done by calling to this function. 
 *
 * This function allows to define several handlers to configure the
 * TLS support. These handlers are defined per profile which means
 * they are global to all TLS profile request received.
 *
 * There are an alternative method which provides more control over
 * the TLS process. This is controlled by the following functions:
 * 
 *  - \ref vortex_tls_set_ctx_creation
 *  - \ref vortex_tls_set_default_ctx_creation
 *
 * Previous functions are provided to enable the application layer to
 * provide handlers that are executed to create the TLS context
 * (<b>SSL_CTX</b>), configuring all parameters required. See also
 * \ref VortexTlsCtxCreation handler for more information.
 *
 * Along with previous function, the following ones allows to provide
 * some callbacks that will be called to perform addintionall TLS
 * post-checks. 
 *  
 *  - \ref vortex_tls_set_post_check
 *  - \ref vortex_tls_set_default_post_check
 *
 * <i>NOTE: Using \ref vortex_tls_set_ctx_creation or \ref
 * vortex_tls_set_default_ctx_creation function will cause the
 * following handlers to be not called:
 * 
 *  - certificate_handler (\ref VortexTlsCertificateFileLocator)
 *  - private_key_handler (\ref VortexTlsPrivateKeyFileLocator)
 *
 * This is because providing a function to create the SSL context
 * (SSL_CTX) assumes the application layer on top of Vortex Library
 * wants to take control over the SSL configuration process. This ensures
 * Vortex Library will not do any additional configure operation once
 * create the SSL context (SSL_CTX).</i>
 *  
 * 
 * 
 *
 * @param accept_handler A handler executed to notify user app level
 * that a TLS request was received, allowing to accept or deny it
 * according to the value returned by the handler. You can use NULL
 * value for this parameter. This will make Vortex Library to set the
 * default accept handler which always accept every TLS negotiation.
 * 
 * @param certificate_handler A handler executed to know where is
 * located the certificate file to be used to cipher the session. You
 * can use NULL value for this parameter. This will make Vortex
 * Library to set the default certificate handler which returns a path
 * to a test certificate. It is highly recommended to set this handler,
 * however you can use NULL value under development environment. 
 *
 * @param private_key_handler A handler executed to know where is
 * located the private key file to be used to cipher the session. You
 * can use NULL value for this parameter. This will make Vortex Library
 * to set the default private key handler which returns a path to the
 * test private key. It is highly recommended to set this handler,
 * however you can use NULL values under development environment.
 *
 * @param ctx The context where the operation will be performed.
 *
 * @return Returns it the current server instance could accept incoming TLS connections.
 */
bool        vortex_tls_accept_negotiation (VortexCtx                       * ctx,
					   VortexTlsAcceptQuery              accept_handler, 
					   VortexTlsCertificateFileLocator   certificate_handler,
					   VortexTlsPrivateKeyFileLocator    private_key_handler) 
{
	VortexTlsCtx * tls_ctx;

	/* check and init TLS */
	if (! vortex_tls_init (ctx)) {
		vortex_log (VORTEX_LEVEL_WARNING, "unable to initialize TLS library, unable to accept incoming requests");
		return false;
	}

	/* check if the tls ctx was created */
	tls_ctx = vortex_ctx_get_data (ctx, TLS_CTX);
	if (tls_ctx == NULL) 
		return false;

	/* set default handlers */
	tls_ctx->tls_accept_handler      = (accept_handler      != NULL) ? accept_handler      : vortex_tls_default_accept;
	tls_ctx->tls_certificate_handler = (certificate_handler != NULL) ? certificate_handler : vortex_tls_default_certificate;
	tls_ctx->tls_private_key_handler = (private_key_handler != NULL) ? private_key_handler : vortex_tls_default_private_key;

	/* register the profile */
	vortex_profiles_register (ctx,
				  VORTEX_TLS_PROFILE_URI,
				  /* the start handler */
				  NULL, NULL,
				  /* the close handler */
				  NULL, NULL,
				  /* the frame received */
				  NULL, NULL);

	/* register the extended start message to process all incoming 
	 * start messages received. */
	vortex_profiles_register_extended_start (ctx, 
						 VORTEX_TLS_PROFILE_URI,
						 vortex_tls_process_start_msg,
						 NULL);

	vortex_log (VORTEX_LEVEL_DEBUG, "tls profile activated accept handler=%p, certificate handler=%p, private key handler=%p",
		    tls_ctx->tls_accept_handler, 
		    tls_ctx->tls_certificate_handler, 
		    tls_ctx->tls_private_key_handler);

	/* well, we are prepare to tls-ficate! */
	return true;
}

/** 
 * @brief Returns the SSL object associated to the given connection.
 *
 * Once a TLS negotiation has finished, the SSL object, representing
 * the TLS session, according to the OpenSSL documentation, is stored
 * on the connection. This function allows to return that reference. 
 *
 * @param connection The connection with a TLS session activated.
 * 
 * @return The SSL object reference or NULL if not defined. The
 * function returns NULL if TLS profile is not activated (at
 * compilation time).
 */
axlPointer         vortex_tls_get_ssl_object             (VortexConnection * connection)
{
	/* return the ssl object which is stored under the key:
	 * ssl-data:ssl */
	return vortex_connection_get_data (connection, "ssl-data:ssl");
}

/** 
 * @brief Allows to return the certificate digest for the remote peer,
 * once an TLS session is activated (this is also called the
 * certificate fingerprint).
 * 
 * @param connection The connection where a TLS session was activated,
 * and a message digest is required over the certificate provided by
 * the remote peer. 
 * 
 * @param method This is the digest method. Current only \ref VORTEX_SHA1
 * is supported.
 * 
 * @return A newly allocated fingerprint or NULL if it fails. The
 * function should return NULL. If NULL is returned there is a TLS
 * error (certificate not provided) or the system is out of memory. If
 * the case the value is defined,
 */
char             * vortex_tls_get_peer_ssl_digest        (VortexConnection   * connection, 
							  VortexDigestMethod   method)
{
	/* variable declaration */
	int             iterator;
	unsigned int    message_size;
	unsigned char   message [EVP_MAX_MD_SIZE];
	char          * result;
	
	/* ssl variables */
	SSL          * ssl;
	X509         * peer_cert;
	const EVP_MD * digest_method = NULL;

	/* get ssl object */
	ssl = vortex_tls_get_ssl_object (connection);
	if (ssl == NULL)
		return NULL;

	/* get remote peer */
	peer_cert = SSL_get_peer_certificate (ssl);
	if (peer_cert == NULL) {
		return NULL;
	}

	/* configure method digest */
	switch (method) {
	case VORTEX_SHA1:
		digest_method = EVP_sha1 ();
		break;
	case VORTEX_MD5:
		digest_method = EVP_md5 ();
		break;
	case VORTEX_DIGEST_NUM:
		/* do nothing */
		return NULL;
	}
	
	/* get the message digest and check */
	if (! X509_digest (peer_cert, digest_method, message, &message_size)) {
		return NULL;
	} 

	/* allocate enough memory for the result */
	result = axl_new (char, (message_size * 3) + 1);

	/* translate value returned into an octal representation */
	for (iterator = 0; iterator < message_size; iterator++) {
#if defined(AXL_OS_WIN32)  && ! defined(__GNUC__)
		sprintf_s (result + (iterator * 3), (message_size * 3), "%02X%s", 
			       message [iterator], 
			       (iterator + 1 != message_size) ? ":" : "");
#else
		sprintf (result + (iterator * 3), "%02X%s", 
			     message [iterator], 
			     (iterator + 1 != message_size) ? ":" : "");
#endif
	}

	return result;
}

/** 
 * @brief Allows to create an MD5 digest from the provided string.
 * 
 * @param string The string to digest.
 *
 * @param method The digest method to be used for the resulting
 * output.
 * 
 * @return A hash value that represents the string provided.
 */
char             * vortex_tls_get_digest                 (VortexDigestMethod   method,
							  const char         * string)
{
	/* use sized version */
	return vortex_tls_get_digest_sized (method, string, strlen (string));
}

/** 
 * @brief Allows to create an MD5 digest from the provided string,
 * configuring the size of the string to be calculated. This function
 * performs the same action as \ref vortex_tls_get_digest, but
 * allowing to configure the size of the input buffer to be used to
 * configure the resulting digest.
 * 
 * @param method The digest method to be used for the resulting
 * output.
 *
 * @param content The content to digest.
 *
 * @param content_size The amount of data to be taken from the input
 * provided.
 * 
 * @return A hash value that represents the string provided.
 */
char             * vortex_tls_get_digest_sized           (VortexDigestMethod   method,
							  const char         * content,
							  int                  content_size)
{
	char          * result = NULL;
	unsigned char   buffer[EVP_MAX_MD_SIZE];
	EVP_MD_CTX      mdctx;
	const EVP_MD  * md = NULL;
	int             iterator;
#ifdef __SSL_0_97__
	unsigned int   md_len;
#else
	unsigned int   md_len = EVP_MAX_MD_SIZE;
#endif
		
	if (content == NULL)
		return NULL;

	/* create the digest method */
	/* configure method digest */
	switch (method) {
	case VORTEX_SHA1:
		md = EVP_sha1 ();
		break;
	case VORTEX_MD5:
		md = EVP_md5 ();
		break;
	case VORTEX_DIGEST_NUM:
		/* do nothing */
		return NULL;
	}
	
	/* add all digest */
	/* OpenSSL_add_all_digests(); */
		
#ifdef __SSL_0_97__
	EVP_MD_CTX_init(&mdctx);
	EVP_DigestInit_ex(&mdctx, md, NULL);
	EVP_DigestUpdate(&mdctx, content, content_size);
	EVP_DigestFinal_ex(&mdctx, buffer, &md_len);
	EVP_MD_CTX_cleanup(&mdctx);
#else
	EVP_DigestInit (&mdctx, md);
	EVP_DigestUpdate (&mdctx, content, content_size);
	EVP_DigestFinal (&mdctx, buffer, &md_len);
#endif

	/* translate it into a octal format, allocate enough memory
	 * for the result */
	result = axl_new (char, (md_len * 3) + 1);

	/* translate value returned into an octal representation */
	for (iterator = 0; iterator < md_len; iterator++) {
#if defined(AXL_OS_WIN32) && ! defined(__GNUC__)
		sprintf_s (result + (iterator * 3), (md_len * 3), "%02X%s", 
			       buffer [iterator], 
			       (iterator + 1 != md_len) ? ":" : "");
#else	
		sprintf (result + (iterator * 3), "%02X%s", 
			 buffer [iterator], 
			 (iterator + 1 != md_len) ? ":" : "");
#endif
	}
	
	/* return the digest */
	return result;
}

/** 
 * @internal Function used by the main module to cleanup the tls
 * module on exit (dealloc all memory used by openssl).
 */
void               vortex_tls_cleanup (VortexCtx * ctx)
{
	/* remove all cyphers */
	EVP_cleanup ();

	return;
}

int vortex_tls_auto_tlsfixate_conection (VortexCtx               * ctx,
					 VortexConnection        * connection,
					 VortexConnection       ** new_conn,
					 VortexConnectionStage    stage,
					 axlPointer               user_data)
{
	VortexTlsCtx * tls_ctx = vortex_ctx_get_data (ctx, TLS_CTX);

	/* check for auto TLS negotiation */
	if (tls_ctx != NULL && tls_ctx->connection_auto_tls) {
		/* seems that current Vortex Library have TLS
		 * profile built-in support and auto TLS is
		 * activated */
		connection = vortex_tls_start_negotiation_sync (connection,
								tls_ctx->connection_auto_tls_server_name,
								NULL, NULL);
		if (!vortex_connection_is_ok (connection, false)) {
			return -1;
		}
		
		/* the connection is ok, check if the TLS
		 * profile was activated, but only if auto tls
		 * allow failures is not set */
		if (! tls_ctx->connection_auto_tls_allow_failures) {
			if (! vortex_connection_is_tlsficated (connection)) {
				__vortex_connection_set_not_connected (connection,
								       "Automatic TLS-fication have failed, the connection have been flagged to be closed due to not allowing TLS failure settings");
				return -1;
			} /* end if */
		} /* end if */

		/* update caller connection */
		(*new_conn) = connection;

		/* return 2 to signal the caller to update the
		 * connection reference */
		return 2;
	}

	/* auto-tls ok or nothing to do */
	return 1;
}

/** 
 * @brief Allows to activate TLS profile automatic negotiation for every connection created.
 * 
 * Once a user application is developed using Vortex Library it could
 * be interesting to instruct Vortex Library to automatically
 * negotiate the TLS profile for every connection created. This will
 * make that every call to \ref vortex_connection_new will return not
 * only an instance already connected but also with the TLS profile
 * already activated.
 * 
 * This allows to take advantage of the support developed to create
 * and wait for a \ref VortexConnection to be created rather than
 * having two steps at the user space: first create the connection and
 * the TLS-fixate it with \ref vortex_tls_start_negotiation.
 *
 * The function allows to specify the optional serverName value to be
 * used when \ref vortex_tls_start_negotiation is called. The values
 * set on this function will make effect to all connections created.
 * 
 * Once a \ref VortexConnection "connection" is created, the TLS
 * profile negotiation could fail. This is because the remote peer could be not
 * accepting TLS request, or the serverName request is not accepted or
 * any other issue. 
 * 
 * This could be a security problem because there is no difference from
 * using a \ref VortexConnection with TLS profile activated from other
 * one without it. This could cause user application to start using a
 * connection that is successfully connected but the TLS profile have
 * actually failed, sending and receiving text in plain mode.
 * 
 * The parameter <b>allow_tls_failures</b> allows to configure what is
 * the default action is to be taken on TLS failures. By default, if
 * TLS profile negotiation fails, the connection is closed, returning that the TLS
 * profile have failed. 
 * 
 * Using a true value allows to still keep on working even if the TLS
 * profile negotiation have failed.
 *
 * By default, Vortex Library have auto TLS feature disabled.
 * 
 * @param ctx The context where the operation will be performed.
 * 
 * @param enabled true to activate the automatic TLS profile
 * negotiation for every connection created, false to disable it.
 *
 * @param allow_tls_failures Configure how to handle errors produced
 * while activating automatic TLS negotiation.
 *
 * @param serverName The server name value to be passed in to \ref
 * vortex_tls_start_negotiation. If the received is not NULL the
 * function will perform a local copy
 *
 * <i><b>NOTE:</b> If current Vortex Library doesn't have built-in
 * support for TLS profile, automatic TLS profile negotiation will
 * always fail. This means that setting <b>allow_tls_failures </b> to
 * false will cause Vortex Library client peer to always fail to
 * create new connections.</i>
 *
 * <i><b>NOTE2: About could fail during the TLS handshake</b> <br> A
 * TLS handshake could fail at two points: before the tuning start or
 * a failure during the TLS handshake itself. In the second case the error
 * is not recoverable because is not possible to restore the BEEP
 * state on both peers.
 * 
 * In the first case, the connection is still working and BEEP state
 * remains untouched because the error at this phase is caused because
 * the partner peer have denied accepting the TLS handshare by
 * rejecting to create the TLS channel, leaving both peers at the BEEP
 * level.
 *
 * Having this in mind, you must always call to \ref
 * vortex_connection_is_ok after a connection create operation. 
 * </i>
 */
void                vortex_tls_set_auto_tls       (VortexCtx  * ctx,
						   bool         enabled,
						   bool         allow_tls_failures,
						   const char * serverName)
{

	VortexTlsCtx * tls_ctx;

	v_return_if_fail (ctx);

	/* get a reference to the TLS context */
	tls_ctx = vortex_ctx_get_data (ctx, TLS_CTX);
	if (tls_ctx == NULL)
		return;

	/* save boolean values */
	tls_ctx->connection_auto_tls                = enabled;
	tls_ctx->connection_auto_tls_allow_failures = allow_tls_failures;

	/* unref previous values */
	if (tls_ctx->connection_auto_tls_server_name != NULL)
		axl_free (tls_ctx->connection_auto_tls_server_name);

	/* store new value */
	tls_ctx->connection_auto_tls_server_name    = (serverName != NULL) ? axl_strdup (serverName) : NULL;
	
	return;
}

/* @} */