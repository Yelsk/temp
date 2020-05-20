# 0.2版 C++ Web Server

#### 0.2版增加功能实现：

##### Log功能：

- 使用单例模式创建日志系统，对服务器运行状态、错误信息等进行记录
- 实现按天分类、超行分类
- 实现异步写入

##### Log异步模型：

![log线程](D:\codetest\log线程.png)

##### 其他：

- 单例模式使用的线程安全懒汉模式
- 当其他线程调用Log写入方法时，写入信息会先放在阻塞队列，阻塞队列使用的是条件变量实现的生产者消费者模式。Log写入线程会异步的读取阻塞队列中的信息，并将其写入文件流中。
- 互斥锁的使用：
  - 阻塞队列的任何操作都需要加锁。
  - 文件流的写入也需要加锁。

**测试：**

![ServerWithoutLog](D:\codetest\dat\ServerWithoutLog.png)

在关闭Log功能的情况下进行短连接压力测试，QPS为30585。

![ServerWithLog](D:\codetest\dat\ServerWithLog.png)

在开启Log功能的情况下进行短连接压力测试，QPS为9398。