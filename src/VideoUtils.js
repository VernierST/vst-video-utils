class MessageClient {
  constructor() {
    this._nextId = 0;
    this._pending = {};

    this.worker = null;
    this._init = null; // init promise
  }

  // returns: Promise<>
  initWorker(url) {
    return new Promise((resolve,reject) => {
      this.worker = new Worker(url);
      this._init = { resolve, reject };

      this.worker.onmessage = this._onmessage.bind(this);
      this.worker.onerror = this._onerror.bind(this);
    });
  }

  callMethod(method, args) {
    console.assert(this.worker);
    console.assert(method);

    args = args || [];
    const id = this._nextId++;
    const msg = { id, method, args };

    return new Promise((resolve,reject) => {
      this._pending[id] = { resolve, reject };
      return this.worker.postMessage(msg);
    });
  }

  shutdown() {
    if (this.worker) {
      this.worker.terminate();
      this.worker = null;
    }
  }

  _onmessage(e) {
    const msg = e.data;

    if (msg === 'initialized') {
      if (this._init) {
        this._init.resolve();
        this._init = null;
      }
    }
    else {
      let p = this._pending[msg.id];
      console.assert(p);

      if (p) {
        delete this._pending[msg.id];
        if (msg.error) {
          p.reject(msg.error);
        } else {
          p.resolve(msg.result);
        }
      }
    }
  }

  _onerror(e) {
    console.error(e);
    if (this._init) {
      this._init.reject(e);
      this._init = null;
    }
  }
}


export class VideoUtils {
  constructor() {
    this.client = new MessageClient();
  }

  init() {
    return this.client.initWorker('vstvideoutils.js');
  }

  // logs file metadata to the console
  dumpMetaData(db, filename) {
    return this.client.callMethod('dumpMetaData', [db,filename]);
  }

  // returns: Promise<{
  //  avgFrameRate,
  //  realFrameRate, // may be 0
  //  numFrames,     // may be 0
  //  duration,
  //  rotation       // rotation angle in degrees
  //  vidWidth
  //  vidHeight
  // }>
  readMetaData(db, filename) {
    return this.client.callMethod('readMetaData', [db,filename]);
  }


  // returns: Promise<>
  transcodeRotation(db, src, dst) {
    return this.client.callMethod('transcodeRotation', [db,src,dst]);
  }

  // returns: Promise<>
  transmuxStripMeta(db, src, dst) {
    return this.client.callMethod('transmuxStripMeta', [db,src,dst]);
  }


  shutdown() {
    if (this.client) {
      this.client.shutdown();
    }
  }
}
