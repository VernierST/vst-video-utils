// This file handles messages from the client program to the worker
// and sending back responses

self.sendResult = (id, result) => {
  postMessage({ id, result });
};


self.sendError = (id, error) => {
  postMessage({ id, error });
};


self.onmessage = (e) => {
  const msg = e.data;
  let valid = false;

  if (typeof(Module[msg.method]) === 'function') {
    Module[msg.method](msg.id, ...msg.args);
    valid = true;
  }

  if (!valid) {
    sendError(msg.id, `Method doesn't exist: ${msg.method}`);
  }
};

Module.onRuntimeInitialized = () => {
  postMessage('initialized');
};
