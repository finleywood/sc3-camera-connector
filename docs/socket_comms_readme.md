# SC3 Camera Connector Socket Protocol

## Protocol format
*ALL* messages will first have a 32bit unsigned integer (big-endian) as preamble to determine the size of the message.
The actual message content will take the format:
```
    MESSAGE_TYPE <optional_string_argument>
```

With the `<optional_string_argument>` being a simple const char* defined string, delimited from the message type identifier by a single space character. This second argument is unique to the message type and it's format specific to the message type parser itself (although will most commonly be just a simple string).

## Collection flow

First intiate a Collection

Client sends *START_COLLECTION <uid>* `<uid>` here represents a unique identifier passed to uniquely reference the collection as it passes through the pipeline. This will initiate a collection server side and start the whole collection and upload process.

Then, Server will send one of 2 messages, either *COLLECTION_SUCCESS* to indicate a successful frame capture and QR decoding, or *COLLECTION_FAILURE* to indicate QR code detection failed.

The server will then intermediately send a *COLLECTION_THUMNAIL* command, which contains a downsized RGB image in jpeg format as a base64 encoded byte string.

At this point the Client can send either a *START_UPLOAD* with an optional argument of `<code>` which represents a manual input override user side, or they can send *COLLECTION_REJECT*, which will reject the current collection and not save/upload it anywhere, or optionally a *USB_DOWNLOAD* message to skip http upload and download to a configured USB.

The server will then send a corresponding success/failure message, eg. *UPLOAD_SUCCESS*/*UPLOAD_FAILURE* or *USB_DOWNLOAD_SUCCESS*/*USB_DOWNLOAD_FAILURE*.

