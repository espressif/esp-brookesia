# NVS Service

The NVS (Non-Volatile Storage) service provides key-value pair storage functionality, supporting multiple namespaces.

Detailed interface descriptions please refer to [NVS service helper header file](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/nvs.hpp).

## Table of Contents

- [NVS Service](#nvs-service)
  - [Table of Contents](#table-of-contents)
  - [List Entries in Namespace](#list-entries-in-namespace)
  - [Set Key-Value Pairs](#set-key-value-pairs)
  - [Get Key-Value Pairs](#get-key-value-pairs)
  - [Erase Key-Value Pairs](#erase-key-value-pairs)
  - [Notes](#notes)

## List Entries in Namespace

List all entries in the default namespace `storage`:

```bash
svc_call NVS List {}
```

List all entries in a specific namespace:

```bash
svc_call NVS List {"Nspace":"Custom"}
```

## Set Key-Value Pairs

Set key-value pairs in the default namespace `storage`:

```bash
svc_call NVS Set {"KeyValuePairs":{"key1":"value1","key2":2,"key3":true}}
```

Set key-value pairs in a specific namespace:

```bash
svc_call NVS Set {"Nspace":"Custom","KeyValuePairs":{"key4":"value4","key5":5,"key6":false}}
```

## Get Key-Value Pairs

Get all key-value pairs from the default namespace:

```bash
svc_call NVS Get {}
```

Get specific key-value pairs from the default namespace:

```bash
svc_call NVS Get {"Keys":["key1","key2","key3"]}
```

Get all key-value pairs from a specific namespace:

```bash
svc_call NVS Get {"Nspace":"Custom"}
```

Get specific key-value pairs from a specific namespace:

```bash
svc_call NVS Get {"Nspace":"Custom","Keys":["key4","key5","key6"]}
```

## Erase Key-Value Pairs

Erase all key-value pairs in the default namespace:

```bash
svc_call NVS Erase {}
```

Erase specific key-value pairs in the default namespace:

```bash
svc_call NVS Erase {"Keys":["key1","key2","key3"]}
```

Erase all key-value pairs in a specific namespace:

```bash
svc_call NVS Erase {"Nspace":"Custom"}
```

Erase specific key-value pairs in a specific namespace:

```bash
svc_call NVS Erase {"Nspace":"Custom","Keys":["key4","key5","key6"]}
```

## Notes

- The default namespace is `storage`
- Erase operations are irreversible, please use with caution
