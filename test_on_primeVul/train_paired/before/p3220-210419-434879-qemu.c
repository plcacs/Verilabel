void vnc_display_open(const char *id, Error **errp)
{
    VncDisplay *vd = vnc_display_find(id);
    QemuOpts *opts = qemu_opts_find(&qemu_vnc_opts, id);
    SocketAddress **saddr = NULL, **wsaddr = NULL;
    size_t nsaddr, nwsaddr;
    const char *share, *device_id;
    QemuConsole *con;
    bool password = false;
    bool reverse = false;
    const char *credid;
    bool sasl = false;
#ifdef CONFIG_VNC_SASL
    int saslErr;
#endif
    int acl = 0;
    int lock_key_sync = 1;
    int key_delay_ms;
    size_t i;

    if (!vd) {
        error_setg(errp, "VNC display not active");
        return;
    }
    vnc_display_close(vd);

    if (!opts) {
        return;
    }

    reverse = qemu_opt_get_bool(opts, "reverse", false);
    if (vnc_display_get_addresses(opts, reverse, &saddr, &nsaddr,
                                  &wsaddr, &nwsaddr, errp) < 0) {
        goto fail;
    }

    password = qemu_opt_get_bool(opts, "password", false);
    if (password) {
        if (fips_get_state()) {
            error_setg(errp,
                       "VNC password auth disabled due to FIPS mode, "
                       "consider using the VeNCrypt or SASL authentication "
                       "methods as an alternative");
            goto fail;
        }
        if (!qcrypto_cipher_supports(
                QCRYPTO_CIPHER_ALG_DES_RFB, QCRYPTO_CIPHER_MODE_ECB)) {
            error_setg(errp,
                       "Cipher backend does not support DES RFB algorithm");
            goto fail;
        }
    }

    lock_key_sync = qemu_opt_get_bool(opts, "lock-key-sync", true);
    key_delay_ms = qemu_opt_get_number(opts, "key-delay-ms", 1);
    sasl = qemu_opt_get_bool(opts, "sasl", false);
#ifndef CONFIG_VNC_SASL
    if (sasl) {
        error_setg(errp, "VNC SASL auth requires cyrus-sasl support");
        goto fail;
    }
#endif /* CONFIG_VNC_SASL */
    credid = qemu_opt_get(opts, "tls-creds");
    if (credid) {
        Object *creds;
        if (qemu_opt_get(opts, "tls") ||
            qemu_opt_get(opts, "x509") ||
            qemu_opt_get(opts, "x509verify")) {
            error_setg(errp,
                       "'tls-creds' parameter is mutually exclusive with "
                       "'tls', 'x509' and 'x509verify' parameters");
            goto fail;
        }

        creds = object_resolve_path_component(
            object_get_objects_root(), credid);
        if (!creds) {
            error_setg(errp, "No TLS credentials with id '%s'",
                       credid);
            goto fail;
        }
        vd->tlscreds = (QCryptoTLSCreds *)
            object_dynamic_cast(creds,
                                TYPE_QCRYPTO_TLS_CREDS);
        if (!vd->tlscreds) {
            error_setg(errp, "Object with id '%s' is not TLS credentials",
                       credid);
            goto fail;
        }
        object_ref(OBJECT(vd->tlscreds));

        if (vd->tlscreds->endpoint != QCRYPTO_TLS_CREDS_ENDPOINT_SERVER) {
            error_setg(errp,
                       "Expecting TLS credentials with a server endpoint");
            goto fail;
        }
    } else {
        const char *path;
        bool tls = false, x509 = false, x509verify = false;
        tls  = qemu_opt_get_bool(opts, "tls", false);
        if (tls) {
            path = qemu_opt_get(opts, "x509");

            if (path) {
                x509 = true;
            } else {
                path = qemu_opt_get(opts, "x509verify");
                if (path) {
                    x509 = true;
                    x509verify = true;
                }
            }
            vd->tlscreds = vnc_display_create_creds(x509,
                                                    x509verify,
                                                    path,
                                                    vd->id,
                                                    errp);
            if (!vd->tlscreds) {
                goto fail;
            }
        }
    }
    acl = qemu_opt_get_bool(opts, "acl", false);

    share = qemu_opt_get(opts, "share");
    if (share) {
        if (strcmp(share, "ignore") == 0) {
            vd->share_policy = VNC_SHARE_POLICY_IGNORE;
        } else if (strcmp(share, "allow-exclusive") == 0) {
            vd->share_policy = VNC_SHARE_POLICY_ALLOW_EXCLUSIVE;
        } else if (strcmp(share, "force-shared") == 0) {
            vd->share_policy = VNC_SHARE_POLICY_FORCE_SHARED;
        } else {
            error_setg(errp, "unknown vnc share= option");
            goto fail;
        }
    } else {
        vd->share_policy = VNC_SHARE_POLICY_ALLOW_EXCLUSIVE;
    }
    vd->connections_limit = qemu_opt_get_number(opts, "connections", 32);

#ifdef CONFIG_VNC_JPEG
    vd->lossy = qemu_opt_get_bool(opts, "lossy", false);
#endif
    vd->non_adaptive = qemu_opt_get_bool(opts, "non-adaptive", false);
    /* adaptive updates are only used with tight encoding and
     * if lossy updates are enabled so we can disable all the
     * calculations otherwise */
    if (!vd->lossy) {
        vd->non_adaptive = true;
    }

    if (acl) {
        if (strcmp(vd->id, "default") == 0) {
            vd->tlsaclname = g_strdup("vnc.x509dname");
        } else {
            vd->tlsaclname = g_strdup_printf("vnc.%s.x509dname", vd->id);
        }
        qemu_acl_init(vd->tlsaclname);
    }
#ifdef CONFIG_VNC_SASL
    if (acl && sasl) {
        char *aclname;

        if (strcmp(vd->id, "default") == 0) {
            aclname = g_strdup("vnc.username");
        } else {
            aclname = g_strdup_printf("vnc.%s.username", vd->id);
        }
        vd->sasl.acl = qemu_acl_init(aclname);
        g_free(aclname);
    }
#endif

    if (vnc_display_setup_auth(&vd->auth, &vd->subauth,
                               vd->tlscreds, password,
                               sasl, false, errp) < 0) {
        goto fail;
    }

    if (vnc_display_setup_auth(&vd->ws_auth, &vd->ws_subauth,
                               vd->tlscreds, password,
                               sasl, true, errp) < 0) {
        goto fail;
    }

#ifdef CONFIG_VNC_SASL
    if ((saslErr = sasl_server_init(NULL, "qemu")) != SASL_OK) {
        error_setg(errp, "Failed to initialize SASL auth: %s",
                   sasl_errstring(saslErr, NULL, NULL));
        goto fail;
    }
#endif
    vd->lock_key_sync = lock_key_sync;
    if (lock_key_sync) {
        vd->led = qemu_add_led_event_handler(kbd_leds, vd);
    }
    vd->ledstate = 0;
    vd->key_delay_ms = key_delay_ms;

    device_id = qemu_opt_get(opts, "display");
    if (device_id) {
        int head = qemu_opt_get_number(opts, "head", 0);
        Error *err = NULL;

        con = qemu_console_lookup_by_device_name(device_id, head, &err);
        if (err) {
            error_propagate(errp, err);
            goto fail;
        }
    } else {
        con = NULL;
    }

    if (con != vd->dcl.con) {
        unregister_displaychangelistener(&vd->dcl);
        vd->dcl.con = con;
        register_displaychangelistener(&vd->dcl);
    }

    if (saddr == NULL) {
        goto cleanup;
    }

    if (reverse) {
        if (vnc_display_connect(vd, saddr, nsaddr, wsaddr, nwsaddr, errp) < 0) {
            goto fail;
        }
    } else {
        if (vnc_display_listen(vd, saddr, nsaddr, wsaddr, nwsaddr, errp) < 0) {
            goto fail;
        }
    }

    if (qemu_opt_get(opts, "to")) {
        vnc_display_print_local_addr(vd);
    }

 cleanup:
    for (i = 0; i < nsaddr; i++) {
        qapi_free_SocketAddress(saddr[i]);
    }
    for (i = 0; i < nwsaddr; i++) {
        qapi_free_SocketAddress(wsaddr[i]);
    }
    return;

fail:
    vnc_display_close(vd);
    goto cleanup;
}