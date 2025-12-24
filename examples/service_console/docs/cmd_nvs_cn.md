# NVS 服务

NVS（Non-Volatile Storage，非易失性存储）服务提供了键值对存储功能，支持多个命名空间。

详细的接口说明请参考 [NVS 服务帮助头文件](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/nvs.hpp)。

## 目录

- [NVS 服务](#nvs-服务)
  - [目录](#目录)
  - [列出命名空间中的条目](#列出命名空间中的条目)
  - [设置键值对](#设置键值对)
  - [获取键值对](#获取键值对)
  - [删除键值对](#删除键值对)
  - [注意事项](#注意事项)

## 列出命名空间中的条目

列出默认命名空间 `storage` 中的所有条目：

```bash
svc_call NVS List {}
```

列出指定命名空间中的所有条目：

```bash
svc_call NVS List {"Nspace":"Custom"}
```

## 设置键值对

在默认命名空间 `storage` 中设置键值对：

```bash
svc_call NVS Set {"KeyValuePairs":{"key1":"value1","key2":2,"key3":true}}
```

在指定命名空间中设置键值对：

```bash
svc_call NVS Set {"Nspace":"Custom","KeyValuePairs":{"key4":"value4","key5":5,"key6":false}}
```

## 获取键值对

获取默认命名空间中的所有键值对：

```bash
svc_call NVS Get {}
```

获取默认命名空间中的指定键值对：

```bash
svc_call NVS Get {"Keys":["key1","key2","key3"]}
```

获取指定命名空间中的所有键值对：

```bash
svc_call NVS Get {"Nspace":"Custom"}
```

获取指定命名空间中的指定键值对：

```bash
svc_call NVS Get {"Nspace":"Custom","Keys":["key4","key5","key6"]}
```

## 删除键值对

删除默认命名空间中的所有键值对：

```bash
svc_call NVS Erase {}
```

删除默认命名空间中的指定键值对：

```bash
svc_call NVS Erase {"Keys":["key1","key2","key3"]}
```

删除指定命名空间中的所有键值对：

```bash
svc_call NVS Erase {"Nspace":"Custom"}
```

删除指定命名空间中的指定键值对：

```bash
svc_call NVS Erase {"Nspace":"Custom","Keys":["key4","key5","key6"]}
```

## 注意事项

- 默认命名空间为 `storage`
- 删除操作不可恢复，请谨慎操作
