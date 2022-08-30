# 使用nghttp2实现rest server

## About
本文主要讨论如何使用libnghttp2来实现一个http rest server, 并简要讨论http rest server应该具有什么样的功能， 以满足isulad的需要。

* 首先， 我们比较一下`http/1.1`与`http/2`的主要区别在哪
* 然后我们分析nghttp2的主要特点与重要的API
* 接着通过一张简要的架构设计图来介绍一下这个rest server会有哪些组件
* 最后通过分析下isulad的一些处理流程，来看看isulad将会怎样使用该rest server， 分析的一些重要流程如下：当普通rest request和流式rest request到达时, rest server怎样处理；绑定的endpoint handler怎样调用下层具体执行容器操作的callback等场景。 


## 1. Features In Http/2
### 1.1 二进制框架层
http1.1和2.0 最大的区别是2.0多出的一个二进制框架层。与 http1.1把所有请求和响应作为纯文本不同，http2 使用二进制框架层把所有消息封装成二进制，且仍然保持http语法，消息的转换让http2能够尝试http1.1所不能的传输方式。

要理解为什么会出现二进制框架层, 可以从没有二进制框架层的http/1.1有什么问题入手：这个问题就是*http/1.1的流水线和队头阻塞*。

**流水线** 即http/1.1相对于http/1.0做出的改进， 通过增加keep-alive头部， 不同的request可以复用同一条tcp长连接，许多request在这一条tcp连接上一次发出，就像在流水线上一样。 tcp建立连接资源消耗大， 所以在高并发的场景下这是一个很重要的改进， 现代的web browser和流行的http库都是默认开启keep-alive的。 可以写一个简单的http/1.1 client 来做实验， 只要是请求同一个server， 那么所有的request都是从同一个socket fd上发出去的。

**队头阻塞** 即http/1.1使用长连接和流水线之后出现的问题， 如果流水线上的第一个request发生了阻塞， 那么这条连接上的后面的request都会被阻塞直到第一个request发送出去或者超时。添加并行的tcp连接能够减轻这个问题，但是tcp连接的数量是有限的，每个新的连接需要额外的资源， 这种并行连接就是http/1.0的处理方式。

**二进制框架层** 理解http/2的二进制框架层之前， 就是理解一些http/2的抽象概念：
* Session: session是tcp connection的抽象， 建立connection之后， session就出现了， 一个session会管理这个connection， connection error 或者 end之后， 当前的session也会关闭。Session要管理一个或者多个stream， 这就是二进制框架层带来的multiplexing功能.
* Stream: 已知tcp提供流式连接， 那么tcp stream与这里的stream的区别是什么呢？这里的Stream通过多路复用， 跑在同一个tcp stream上，也就是说， 这里的stream提供原tcp stream一样对功能， 但是抽象了多路复用这个细节。
* Message: Message即http/2标准定义对proto部分， 这一部分与http/1.1差别不大， 略有修改。从Stream上recv的data是无结构的raw bit stream， 序列化成Message； 相反的， Message通过反序列化成raw bit stream, 向Stream上send. Message包括request message和response message。
* Frame: 上述的raw bit stream要通过data chunk来发送和接收， Frame就是data chunk的抽象。frame data chunk是最小通信单位，以二进制压缩格式存放内容。

### 1.2 Stream优先级
不会使用到， 暂时没有总结， TODO

### 1.3 服务端 push
不会使用到， 暂时没有总结， TODO

## 2. Features In nghttp2
nghttp2是HTTP/2 protocol的一个c实现， 


### 2.1 Low Level http library
nghttp2的地位属于是一个low level的http2库， 它并不针对client， server设计， 它只是提供http/2的那些抽象概念的具体实现， 至于如何实现client或者server， 由用户自己负责。具体一点来说， 你在编写client的时候， 可以使用nghttp2的api来构建一个message对象， 然后使用api发送这个对象， nghttp2会帮你完成序列化， split into data chunk, send over stream等工作. 

可以与其他high level http library来一个对比， 这里与go来一个对比：
```go
package main

import (
    "net/http"
)

client := &http.Client{
	CheckRedirect: redirectPolicyFunc,
}

req, err := http.NewRequest("GET", "http://www.google.com", nil)
req.Header.Add("Language", "English")
resp, err := client.Do(req)
```

对比之下， nghttp2不提供使用`client, request, response`这些对象的方便的接口， client建立连接和io这部分也需要自己完成：
```c
static int connect_to(const char *host, uint16_t port) {
  struct addrinfo hints;
  int fd = -1;
  int rv;
  char service[NI_MAXSERV];
  struct addrinfo *res, *rp;
  snprintf(service, sizeof(service), "%u", port);
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  rv = getaddrinfo(host, service, &hints, &res);
  if (rv != 0) {
    dief("getaddrinfo", gai_strerror(rv));
  }
  for (rp = res; rp; rp = rp->ai_next) {
    fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd == -1) {
      continue;
    }
    /*
    ** connect to a server, get the socketfd.
    */
    while ((rv = connect(fd, rp->ai_addr, rp->ai_addrlen)) == -1 &&
           errno == EINTR)
      ;
    if (rv == 0) {
      break;
    }
    close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  return fd;
}

static ssize_t send_callback(nghttp2_session *session, const uint8_t *data,
                             size_t length, int flags, void *user_data) {
  struct Connection *connection;
  int rv;
  (void)session;
  (void)flags;

  connection = (struct Connection *)user_data;
  /*
  ** do real io on the socketfd
  */
  rv = write(connection->fd, data, (int)length);
  if (rv <= 0) {
    perror("write error");
    return rv;
  }
  printf("write success: %d\n", rv);
  return rv;
}
```

总结一下client发送一次request需要完成的操作： 
* 建立连接， 拿到socketfd
* 构建一个request对象， 调用nghttp2_submit_request把request提交到待发送队列，这时候不会发生io. 
* 在fd上使用poll, 当fd可写时调用nghttp2_session_send, 该api会导致一系列callback被调用， 包括反序列化的callback, send_callback等。 nghttp2_session_send会选择优先级最高的frame来发送
* nghttp2_session_send api会导致我们注册的send_callback被调用， 这时候发送的data是待发送队列中的request反序列化之后的结果
* 由于可写， 可以非阻塞的在fd上发送这些保存在nghttp buffer上的数据

![picture: client send request process]()

### 2.2. High Level Application
在nghttp2仓库下， 还提供了几个基于libnghttp2的high level application:
* nghttp-client, c++实现的一个http/2客户端
* nghttpd, c++实现的一个multi-thread static web server
* nghttpx, c++实现的一个multi-thread http reverse proxy
* nghttp-asio, 使用c++实现的一个web server框架， 与所有的现代web server框架类似， 支持使用匿名函数注册handler， 可以非常方便的完成编写handler， 注册路由， 配置server等操作。

isulad要实现的rest server就与nghttp-asio一样，这里简单看下nghttp-asio怎么启动一个web server, 并与go做一下对比：
```c++
int main(int argc, char *argv[]) {
  try {
    // Check command line arguments.
    if (argc < 4) {
      std::cerr
          << "Usage: asio-sv <address> <port> <threads> [<private-key-file> "
          << "<cert-file>]\n";
      return 1;
    }

    boost::system::error_code ec;

    std::string addr = argv[1];
    std::string port = argv[2];
    std::size_t num_threads = std::stoi(argv[3]);

    http2 server;

    server.num_threads(num_threads);

    /*
    ** register route with a handler
    */
    server.handle("/", [](const request &req, const response &res) {
      res.write_head(200, {{"foo", {"bar"}}});
      res.end("hello, world\n");
    });

    if (argc >= 6) {

      if (server.listen_and_serve(ec, NULL, addr, port)) {
        std::cerr << "error: " << ec.message() << std::endl;
      }
    } else {
      if (server.listen_and_serve(ec, addr, port)) {
        std::cerr << "error: " << ec.message() << std::endl;
      }
    }
  } catch (std::exception &e) {
    std::cerr << "exception: " << e.what() << "\n";
  }

  return 0;
}

```

```go
import (
   "fmt"
   "net/http"
)

func main () {
   http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
    fmt.Fprintf(w, "Hello")
   })
   http.ListenAndServe(":8000", nil)
}
```
这两个web server框架的使用看起来非常的类似， 即使使用了不同的语言。


### 2.3 部分API介绍
 TODO


## 3 Rest Server Design
nghttp2的实现思路是， 针对完全抽象的http/2标准做了具体的实现， 上面讨论的一些http/2中的抽象概念在nghttp2中都有对应的组件： 
* Session -> `struct nghttp2_session`
* Stream -> `struct nghttp2_stream`
* Message -> 没有request， response的具体结构体， 因为Message被分成了粒度更细的数据结构， 比如header, body. `  rv = nghttp2_submit_response(session, stream_id, nva, nvlen, &data_prd);`
* Frame -> `struct nghttp2_headers struct nghttp2_priority`

nghttp2的实现不是专门为了client或是server，它的实现更加的抽象， 为了实现rest server， 思路就是：依然利用nghttp2的这些通用的组件， 在其基础上添加更加具体的实现， 在每一个组件上添加更多的数据成员和代码逻辑来实现一个服务器的功能。

因此， rest server的主要模块如下：
* Server, 这一部分主要负责监听和建立连接， 这一部分的逻辑在nghttp2中是缺失的， 因此需要从0实现
* I/O Event, 这一部分用libevent来实现异步i/o, 用libevent的api来代替传统的socket相关系统调用， 因此其他涉及到使用网络I/O的组件不是主动I/O, 而是定义callback函数，并注册到本模块上， 当事件发生时， 本模块会驱动其他组件完成对应操作
* Session Context, 这一部分是nghttp_session的更加具体的实现， 当建立连接之后， 会创建一个session context, 一个session context就是一条tcp连接。 session context的主要职责就是定义一系列的callback， 当某些特定i/o事件发生，应该负责建立其他的对象，举个例子： 当装载着headers的frame到达的时候， 这时候意味着一条新的Stream建立了， 应该创建一个新的stream对象并管理它。 额外举个例子：当装载着data的request body到达的时候， 这时候应该调用事先定义好的处理request body的代码， 比如request body是json string， 那么就应该调marshal json string的代码. 
* Stream Context, 当Session Context发现了一个request header的时候，这标志着一个Stream建立了， 这时候Session Context会建立Stream并管理它，Stream Context会唯一绑定nghttp2 Stream对象。 Stream Context对象会负责创建Request和Response对象。
* Mux, 这是路由模块，通过request path, 路由到注册的handler上去， 让对应的handler来处理request。
* Request and Response, handler通过调用这两个模块上的方法来处理request并响应。





## 4 Handling Process