/* ios-sweet: dropbear for the phone's USB link (.github/workflows/ssh.yml). Public-key login only. */
#define DROPBEAR_SVR_PASSWORD_AUTH 0
#define DROPBEAR_SVR_PAM_AUTH 0
#define DROPBEAR_CLI_PASSWORD_AUTH 0
#define DROPBEAR_X11FWD 0
#define DROPBEAR_SVR_AGENTFWD 0
#define DROPBEAR_CLI_AGENTFWD 0
#define DEFAULT_PATH "/var/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"
#define DEFAULT_ROOT_PATH "/var/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
#define DSS_PRIV_FILENAME "/private/etc/dropbear/dropbear_dss_host_key"
#define RSA_PRIV_FILENAME "/private/etc/dropbear/dropbear_rsa_host_key"
#define ECDSA_PRIV_FILENAME "/private/etc/dropbear/dropbear_ecdsa_host_key"
#define ED25519_PRIV_FILENAME "/private/etc/dropbear/dropbear_ed25519_host_key"
