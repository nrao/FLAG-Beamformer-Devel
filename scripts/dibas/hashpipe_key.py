import shm
import os

def hashpipe_ipckey(proj_id, verbose = False):
    """
     * Get the base key to use for hashpipe databufs and statusbufs.
     *
     * The base key is obtained by calling the ftok function, using the value of
     * $HASHPIPE_KEYFILE, if defined, or $HOME from the environment or, if $HOME is
     * not defined, by using "/tmp".  By default (i.e. HASHPIPE_KEYFILE does not
     * exist in the environment), this will create and connect to a user specific
     * set of shared memory buffers (provided $HOME exists in the environment), but
     * if desired users can connect to any other set of memory buffers by setting
     * HASHPIPE_KEYFILE appropraitely.
     *
     * The proj_id key is used to allow the caller to have mulitple per-user keys.
     * This function is declared static, so only the functions in this file (i.e.
     * hashpipe_databuf_key() and hashpipe_status_key() can call it.
     *
     * HASHPIPE_KEY_ERROR is returned on error.
    """
    key = -1
    keyfile = os.environ.get("HASHPIPE_KEYFILE")
    if keyfile is None:
        keyfile = os.environ.get("HOME")
        if keyfile is None:
            keyfile = "/tmp"

    #ifdef HASHPIPE_VERBOSE
    #    fprintf(stderr,
    #            "using pathname '%s' and proj_id '%d' to generate base IPC key\n",
    #            keyfile, proj_id&0xff);
    #endif
    if verbose:
        print "using pathname '%s' and proj_id '%d' to generate base IPC key" % (keyfile, proj_id&0xff)

    #print "keyfile: ", keyfile, " proj_id: ", proj_id
    
    key = shm.ftok(keyfile, proj_id)

    if key == -1:
        print "Error w/ ftok!!!"

    # do I really need to do this?  Yes, if want same results as .c version
    if key < 0:
        key += 2**32

    return key

def hashpipe_databuf_key(instance_id):
    return hashpipe_key(instance_id, "HASHPIPE_DATABUF_KEY", 0x80)

def hashpipe_status_key(instance_id):
    return hashpipe_key(instance_id, "HASHPIPE_STATUS_KEY", 0x40)

def hashpipe_key(instance_id, env_key, base):
    """
    * Get the base key to use for the hashpipe buffer.
    * The lower 6 bits of the instance_id parameter are used to allow multiple
    * instances to run under the same user without collision.  The same
    * instance_id can and should be used for databuf keys and status keys.
    """
    status_key = os.environ.get(env_key)
    if status_key is not None:
        key = int(status_key, 16)
    else:   
        # Use instance_id to generate proj_id for hashpipe_ipckey.
        # Status proj_id is 01XXXXXX (binary) where XXXXXX are the 6 LSbs
        # of instance_id.
        key = hashpipe_ipckey((instance_id&0x3f)|base)
    return key    
    
def hashpipe_status_semname(instance_id):
    """
    For:
       * instance_id = 0
       * $HOME = /users/pmargani
    Returns:
       * users_pmargani_hashpipe_status_0
    """

    keyfile = os.environ.get("HASHPIPE_STATUS_SEMNAME")
    if keyfile is not None:
        return keyfile

    keyfile = os.environ.get("HASHPIPE_KEYFILE")
    if keyfile is None:
        keyfile = os.environ.get("HOME")
        if keyfile is None:
            keyfile = "/tmp"
        
    # remove the leading '/'
    if keyfile[0] == '/':
        keyfile = keyfile[1:]

    # replace / w/ _
    keyfile = keyfile.replace('/', '_')

    return "%s_hashpipe_status_%d" % (keyfile, instance_id) 
