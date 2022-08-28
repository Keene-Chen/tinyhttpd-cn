### 简介
---
tinyhttpd 是 J. David Blackstone 在1999年使用C语言写的轻量级 HTTP Server

### 阅读须知
---
1. 源码下载: [tinyhttpd-0.1.0](https://sourceforge.net/projects/tiny-httpd/)

2. 源码是在 Solaris 2.6上编译运行，Linux 需要删除 `makefile` 中的 `-lsocket`

3. 测试 CGI 时需要本机安装 PERL，同时安装 perl-cgi，当然可以使用其他脚本语言编写 CGI

4. 阅读顺序 (括号内数字为源码行数)

    `main(475) -> startup(420) -> accept_request(51) -> execute_cgi(204)`

5. 能学习到什么？

    a. Linux 系统调用 API，即如何读取文件,如何修改文件权限等

    b. Linux 多进程，进程间通信(pipe 管道)

    c. Linux 多线程，`pthread` 库使用

    d. Linux 网络编程，socket 建立连接

### 项目结构
---
```
.
├── tinyhttpd-0.1.0_note // 源码注释目录
│   ├── htdocs           // 网页资源目录
│   │   ├── check.cgi    // perl-CGI脚本
│   │   ├── color.cgi
│   │   ├── date.cgi     // bash-CGI脚本
│   │   ├── index.html   // 网站首页
│   │   └── README       
│   ├── httpd.c          // HTTP Server
│   ├── Makefile
│   ├── README
│   └── simpleclient.c   // HTTP Client
├── tinyhttpd-0.1.0_src  // 源码目录
│   ├── htdocs
│   │   ├── check.cgi
│   │   ├── color.cgi
│   │   ├── index.html
│   │   └── README
│   ├── httpd.c
│   ├── Makefile
│   ├── README
│   └── simpleclient.c
└── README
```

### Q&A
---
1. 何时阅读项目？
    - 本项目是个人学习完 Linux 网络编程时进行阅读

2. 读不懂怎么办？
    - 如果是刚学习完C语言基础，不建议深入阅读，可以从理解函数如何封装、指针的运用、写项目的逻辑等方面学习
    - 有一定C语言编程基础(本人阶段)，可通过思维导图、白板等辅助工具先理解项目整体逻辑，遇到相关API函数时查 [man](https://man7.org/linux/man-pages/index.html) 手册

3. 为什么写本项目？
    - 强化相关API使用，以及理解HTTP Server 工作原理
    - 熟悉 [Doxygen](https://baike.baidu.com/item/Doxygen/1366536) 文档注释规范
    - 熟悉 [MarkDown](https://baike.baidu.com/item/markdown) 语法

### 参考引用
---
1. [Tinyhttpd](https://github.com/EZLippi/Tinyhttpd)
2. [HTTP服务器的本质:tinyhttpd源码分析及拓展](https://www.bbsmax.com/A/Vx5M1VL95N/)
3. [Tinyhttpd精读解析](https://www.cnblogs.com/nengm1988/p/7816618.html)

### 原版权信息(机翻) 
---
该软件的版权归 J. David Blackstone 1999 年所有。允许
被授予重新分发和修改本软件的条款下
GNU 通用公共许可证，可在 http://www.gnu.org/ 获得。

如果您使用此软件或检查代码，我将不胜感激
知道并且会很高兴听到
jdavidb@sourceforge.net。

这个软件不是生产质量的。它没有保修
任何形式的，甚至不是对特定适用性的默示保证
目的。我不对可能造成的损害负责
如果您在计算机系统上使用此软件。

我为我的网络课程中的一项作业编写了这个网络服务器
1999. 我们被告知服务器必须至少提供服务
页面，并告诉我们会因为做“额外”而获得额外的荣誉。

Perl 向我介绍了很多 UNIX 功能（我学会了
来自 Perl 的 sockets 和 fork！），以及 O'Reilly 的关于 UNIX 系统的狮子书
电话加上 O'Reilly 关于 CGI 和用 Perl 编写 Web 客户端的书籍得到了
我在想，我意识到我可以让我的网络服务器支持 CGI
小麻烦。

现在，如果你是 Apache 核心组的成员，你可能不是
印象深刻。但是我的教授被吹走了。试试 color.cgi 示例
脚本并输入“chartreuse”。让我看起来比我更聪明，在
任何速率。 :)

阿帕奇不是。但我确实希望这个程序是一个好的
对 http/socket 编程感兴趣的人的教育工具，如
以及 UNIX 系统调用。 （有一些教科书使用管道，
环境变量、分叉等。）

最后一件事：如果您查看我的网络服务器或（您不在
介意？！？）使用它，我会很高兴听到它。请
给我发邮件。我可能不会真正发布重大更新，但如果
我帮你学点东西，我很想知道！

快乐黑客！

J·大卫·布莱克斯通