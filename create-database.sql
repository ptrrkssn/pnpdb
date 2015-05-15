CREATE TABLE `endpoints` (
  `id` int(11) NOT NULL auto_increment,
  `type` int(11) default NULL,
  `name` varchar(30) NOT NULL,
  `port` varchar(10) default NULL,
  `class` varchar(10) default NULL,
  `room` varchar(20) NOT NULL,
  `comment` text,
  PRIMARY KEY  (`id`),
  UNIQUE KEY `name_port_room` USING BTREE (`name`,`port`,`room`)
) DEFAULT CHARSET=latin1;


CREATE TABLE `patches` (
  `id` int(11) NOT NULL,
  `endpoint1` int(11) default NULL,
  `endpoint2` int(11) default NULL,
  `comment` text,
  `date` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  PRIMARY KEY  (`id`),
  UNIQUE KEY `endpoints12` USING BTREE (`endpoint1`,`endpoint2`)
) DEFAULT CHARSET=latin1;

