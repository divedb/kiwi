在 Linux 系统中，/sys/devices/system/cpu/cpu0/cache/index* 目录提供了 每个缓存层（L1、L2、L3等） 的详细信息。这里的 index0、index1 等是 缓存层的索引编号，每一个 indexX 表示一个特定的缓存层。

🔍 各字段的含义（以 /sys/devices/system/cpu/cpu0/cache/index0/ 为例）：
你可以用 cat 查看子文件，例如：

bash
复制
编辑
cat /sys/devices/system/cpu/cpu0/cache/index0/level
cat /sys/devices/system/cpu/cpu0/cache/index0/type
cat /sys/devices/system/cpu/cpu0/cache/index0/size
cat /sys/devices/system/cpu/cpu0/cache/index0/shared_cpu_list
📘 常见字段解释：
文件名	含义
level	缓存层级（1 表示 L1，2 表示 L2，3 表示 L3）
type	Data, Instruction, or Unified（数据/指令/统一）
size	缓存大小（例如 32K, 256K）
shared_cpu_list	哪些 CPU 共享这个缓存，例如 0,1 表示 CPU 0 和 1 共享
coherency_line_size	缓存行大小（通常为 64 字节）
ways_of_associativity	关联度（n-way set associative cache）
number_of_sets	缓存有多少个 sets（用于计算总大小）

✅ 举个例子：
假设你查看：

bash
复制
编辑
cat /sys/devices/system/cpu/cpu0/cache/index0/level
1
cat /sys/devices/system/cpu/cpu0/cache/index0/type
Data
cat /sys/devices/system/cpu/cpu0/cache/index0/size
32K
这说明：

index0 是 L1 数据缓存

每个核心通常有自己的 L1 Data 和 L1 Instruction 缓存（所以你可能看到 index0, index1 都是 level 1，但一个是 Data，一个是 Instruction）

总结：
indexX	表示内容
index0	L1 数据缓存 (Data cache)
index1	L1 指令缓存 (Instruction cache)
index2	L2 缓存
index3	L3 缓存

这些信息由系统启动时 CPU 描述符（cpuid 指令）生成并挂载在 /sys 文件系统中，供程序读取来进行缓存感知优化，如 Facebook Folly 的 CacheLocality 所做的那样。

