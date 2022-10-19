# Network-Programming
网络编程

Ubuntu进入mysql:
  sudo mysql -u root -p
  
创建数据库:
  create database yourdb;
  use yourdb;
  create table user(
    username char(50) NULL, 
    passwd char(50) NULL
  )ENGINE=InnoDB;
  insert into user(username, passwd) values('name', 'passwd');

查看当前数据库和表的内容：
  show databases;
  show users;
  select * from user;

编译之前，首先，需要确认main.cpp里的数据库和mysql数据库配置相同

查看数据库名称和密码：
  cd /etc/mysql
  sudo vim debian.cnf
然后，修改main.cpp中对应的配置
  user和password：根据debia.cnf中的配置对应修改
  databasename: 这是上面创建的数据库名yourdb

编译运行：
  cd WebServer
  sh ./build.sh
  ./server
  
