# Webserver
linux下轻量级webserver
这是基于《linux高性能服务器》一书开发的一个轻量级服务器。
===============
作者希望不断完善使其成为一个稳定的可以实现全部网站和服务器功能的一个轻量级服务器！
主要技术：

* 使用 线程池 + 非阻塞socket + epoll + （reactor&proactor）事件处理的并发模型
* 使用状态机解析HTTP请求报文，支持POST与GET请求解析，并能显示登录注册功能
* 绑定域名，通过端口号可以请求服务器图片和视频文件（文件大小超100m）
* 实现同步/异步（基于阻塞队列）日志系统，监控服务器状态
* 经Webbench可以实现上万的压力连接数据交换
*地址:http://www.dshuoyan.xyz:15678
