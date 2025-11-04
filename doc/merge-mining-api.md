# Merge Mining API

## How to Setup an MergeMining API with full node

1. Clone Coordinate repo from below url
   <pre>https://github.com/AnduroProject/coordinate.git</pre>
2. Build and run coordinate node by following [doc folder](/doc).

## Create Aux Block API
The mining pool to request new block header hashes through rpc

#### Request
```curl
curl --location 'http://RPCUSER:RPCPASSWORD@HOST:PORT/' \
--header 'Content-Type: application/json' \
--data '{
    "jsonrpc": "1.0",
    "id": "curltest",
    "method": "createauxblock",
    "params": ["MINER ADDRESS"]
}'
```
#### Request data types
| Parameter | Type | Description |
| :--- | :--- | :--- |
| `address` | `string` | **Required**. miner address to get coordinate block tx fee. note  |


#### Response
```response
{
    "result": {
        "hash": "75d62dae243c3d957a7d783fe7b5987c875b270af571141f5cea9b70cc460cba",
        "chainid": 2222,
        "previousblockhash": "a85cf18dffc20f576cf4abaae8be6d908e0b8158e7fa60141fee9108ebc73407",
        "coinbasevalue": 0,
        "bits": "207fffff",
        "height": 5,
        "_target": "0000000000000000000000000000000000000000000000000000000000ffff7f"
    }
    "error": null,
    "id": "curltest"
}
```

#### Response data types
| Parameter | Type | Description |
| :--- | :--- | :--- |
| `hash` | `string` | hash of the created block |
| `chainid` | `number` | chain ID for this block|
| `previousblockhash` | `string` | hash of the previous block|
| `coinbasevalue` | `number` | value of the block's coinbase|
| `bits` | `string` | compressed target of the block |
| `height` | `number` | height of the block |
| `_target` | `string` | target in reversed byte order, deprecated |


## Submit Aux Block API
The mining pool to submit the mined auxPoW

#### Request
```curl
curl --location 'http://coordinate:coordinate@seed2.coordinate.mara.technology:18332/' \
--header 'Content-Type: application/json' \
--data '{
            "jsonrpc": "1.0",
            "id": "curltest",
            "method": "submitauxblock",
            "params": ["44d77dba844e1b72c3025de85a3391ad5e333ff94086ebb36bfd94b30db26b4f","020000000001010000000000000000000000000000000000000000000000000000000000000000ffffffff04020f1700ffffffff020000000000000000160014407cc61f58eb05f477438619ea1cba25a6344ea40000000000000000266a24aa21a9ede2f61c3f71d1defd3fa999dfa36953755c690689799962b48bebd836974e8cf901200000000000000000000000000000000000000000000000000000000000000000000000001d6a1efedd1a043cccb275a634fdd2f91916c7f2fa2ab75332410f0defc5822d0000000000000000000000000020ef2fce002e26c846bbc040f575e174d6016fec71af97bd7cd85d4565eddc2b0fac28faecb5e6dfe47747a9abfaa5781e3b3b77a84eace46918bbfde9343278875f298c64ffff7f2002000000"]
}'
```

#### Request data types
| Parameter | Type | Description |
| :--- | :--- | :--- |
| `blockhash` | `string` | **Required**. Bloch header hash that the submitted auxPoW is associated with. |
| `auxpow` | `string` | **Required**. It is the serialized string of the auxiliary proof of work. |


#### Response
```response
{
    "result": true
}
```

#### Response data types
| Parameter | Type | Description |
| :--- | :--- | :--- |
| `result` | `boolean` | status of block submission |
