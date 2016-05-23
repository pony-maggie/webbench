#WebBench

Webbench是一个在linux下使用的非常简单的网站压测工具。它使用fork()模拟多个客户端同时访问我们设定的URL，测试网站在压力下工作的性能，最多可以模拟3万个并发连接去测试网站的负载能力。

Webbech能测试处在相同硬件上,不同服务的性能以及不同硬件上同一个服务的运行状况.webBech的标准测试可以向我们展示服务器的两项 内容:每秒钟相应请求数和每秒钟传输数据量.webbench不但能具有便准静态页面的测试能力,还能对动态页面(ASP,PHP,JAVA,CGI)进 行测试的能力.还有就是他支持对含有SSL的安全网站例如电子商务网站进行静态或动态的性能测试.

[源码下载](http://home.tiscali.cz/~cz210552/webbench.html)


##编译

`make clean`
`sudo make && make install`


##命令行选项：

* -f --force 不需要等待服务器响应 
* -r --reload 发送重新加载请求 
* -t --time  运行多长时间，单位：秒" 
* -p --proxy server:port 
* -c --clients  创建多少个客户端，默认1个" 
* -9 --http09 
* -1 --http10 使用 HTTP/1.0 协议 
* -2 --http11 
* --get 使用 GET请求方法 
* --head 使用 HEAD请求方 
* --options 使用 OPTIONS请求方法 
* --trace 使用 TRACE请求方法 
* -?/-h --help 打印帮助信息 
* -V --version 显示版本号 


##测试实例:

`webbench -c 500  -t  30   http://192.168.0.99/phpionfo.php`



**注意:webbench 做压力测试时,该软件自身也会消耗CPU和内存资源,为了测试准确,请将 webbench 安装在别的服务器上**

