-- LunaChat 数据库初始化脚本（MySQL 8.0+ / 9.x）
-- 用法：mysql -h127.0.0.1 -uroot -p --default-character-set=utf8mb4 -e "source D:/qtproject/LunaChat/LunaChat---/sql/schema.sql"

CREATE DATABASE IF NOT EXISTS luna_chat DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE luna_chat;

-- 用户：password 存 SHA-256 十六进制，不存明文
CREATE TABLE IF NOT EXISTS `user` (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  username VARCHAR(32) NOT NULL,
  password CHAR(64) NOT NULL,
  nickname VARCHAR(32) NOT NULL,
  avatar_path VARCHAR(255) DEFAULT NULL,
  status ENUM('online','busy','dnd','offline') NOT NULL DEFAULT 'offline',
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS friend (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  user_id BIGINT UNSIGNED NOT NULL,
  friend_id BIGINT UNSIGNED NOT NULL,
  status ENUM('pending','accepted') NOT NULL DEFAULT 'pending',
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_friend (user_id, friend_id),
  KEY idx_friend_id (friend_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS chat_group (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  group_name VARCHAR(64) NOT NULL,
  owner_id BIGINT UNSIGNED NOT NULL,
  avatar_path VARCHAR(512) DEFAULT NULL,
  announcement TEXT,
  folder_path VARCHAR(512) DEFAULT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS group_member (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  group_id BIGINT UNSIGNED NOT NULL,
  user_id BIGINT UNSIGNED NOT NULL,
  role ENUM('owner','admin','member') NOT NULL DEFAULT 'member',
  joined_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_group_user (group_id, user_id),
  KEY idx_user (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS message (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  from_user_id BIGINT UNSIGNED NOT NULL,
  to_user_id BIGINT UNSIGNED DEFAULT NULL,
  group_id BIGINT UNSIGNED DEFAULT NULL,
  msg_type ENUM('text','file','voice') NOT NULL DEFAULT 'text',
  content TEXT,
  file_id VARCHAR(64) DEFAULT NULL,
  voice_id VARCHAR(64) DEFAULT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_group_time (group_id, created_at),
  KEY idx_pair_time (from_user_id, to_user_id, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS offline_message (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  to_user_id BIGINT UNSIGNED NOT NULL,
  payload_json TEXT NOT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  delivered TINYINT(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (id),
  KEY idx_to_user (to_user_id, delivered)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS file_record (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  file_id VARCHAR(64) NOT NULL,
  file_name VARCHAR(255) NOT NULL,
  file_size BIGINT UNSIGNED NOT NULL DEFAULT 0,
  from_user_id BIGINT UNSIGNED DEFAULT NULL,
  to_user_id BIGINT UNSIGNED DEFAULT NULL,
  group_id BIGINT UNSIGNED DEFAULT NULL,
  saved_path VARCHAR(512) DEFAULT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_file_id (file_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS login_log (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  user_id BIGINT UNSIGNED NOT NULL,
  login_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  ip VARCHAR(45) DEFAULT NULL,
  PRIMARY KEY (id),
  KEY idx_user_time (user_id, login_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS group_activity (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  group_id BIGINT UNSIGNED NOT NULL,
  creator_id BIGINT UNSIGNED NOT NULL,
  title VARCHAR(128) NOT NULL,
  description TEXT,
  location VARCHAR(128) DEFAULT NULL,
  start_time DATETIME NOT NULL,
  end_time DATETIME NOT NULL,
  signup_deadline DATETIME NOT NULL,
  max_participants INT NOT NULL DEFAULT 0,
  status ENUM('published','ongoing','ended','cancelled') NOT NULL DEFAULT 'published',
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_group_status (group_id, status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS activity_signup (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  activity_id BIGINT UNSIGNED NOT NULL,
  user_id BIGINT UNSIGNED NOT NULL,
  status ENUM('signed','cancelled') NOT NULL DEFAULT 'signed',
  signed_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  cancelled_at DATETIME DEFAULT NULL,
  PRIMARY KEY (id),
  UNIQUE KEY uk_activity_user (activity_id, user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS moment (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  user_id BIGINT UNSIGNED NOT NULL,
  content TEXT,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  deleted TINYINT(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (id),
  KEY idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS moment_image (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  moment_id BIGINT UNSIGNED NOT NULL,
  image_path VARCHAR(512) NOT NULL,
  sort_order INT NOT NULL DEFAULT 0,
  PRIMARY KEY (id),
  KEY idx_moment (moment_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS moment_like (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  moment_id BIGINT UNSIGNED NOT NULL,
  user_id BIGINT UNSIGNED NOT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_moment_user (moment_id, user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS moment_comment (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  moment_id BIGINT UNSIGNED NOT NULL,
  user_id BIGINT UNSIGNED NOT NULL,
  content VARCHAR(500) NOT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_moment (moment_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS match_session (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  session_id VARCHAR(64) NOT NULL,
  user_a BIGINT UNSIGNED NOT NULL,
  user_b BIGINT UNSIGNED NOT NULL,
  mode ENUM('text','voice') NOT NULL DEFAULT 'text',
  status ENUM('active','ended') NOT NULL DEFAULT 'active',
  started_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  ended_at DATETIME DEFAULT NULL,
  PRIMARY KEY (id),
  UNIQUE KEY uk_session_id (session_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS voice_record (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  voice_id VARCHAR(64) NOT NULL,
  from_user_id BIGINT UNSIGNED NOT NULL,
  to_user_id BIGINT UNSIGNED DEFAULT NULL,
  group_id BIGINT UNSIGNED DEFAULT NULL,
  session_id VARCHAR(64) DEFAULT NULL,
  file_name VARCHAR(255) NOT NULL,
  duration_ms INT NOT NULL DEFAULT 0,
  file_size BIGINT UNSIGNED NOT NULL DEFAULT 0,
  format VARCHAR(8) NOT NULL DEFAULT 'ogg',
  bitrate INT NOT NULL DEFAULT 0,
  saved_path VARCHAR(512) DEFAULT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_voice_id (voice_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 演示数据：两个测试账号，密码均为 123456（SHA-256 小写十六进制）
INSERT IGNORE INTO `user` (id, username, password, nickname, status) VALUES
  (1, 'luna', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '月见', 'offline'),
  (2, 'star', '8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92', '星野', 'offline');

-- 默认群"校园大厅"：群主同时写入 group_member.role = owner
INSERT IGNORE INTO chat_group (id, group_name, owner_id) VALUES (1, '校园大厅', 1);
INSERT IGNORE INTO group_member (group_id, user_id, role) VALUES (1, 1, 'owner'), (1, 2, 'member');
-- 阶段11 增量：已有库补列（重复执行会报 duplicate column，可忽略）
ALTER TABLE chat_group ADD COLUMN avatar_path VARCHAR(512) DEFAULT NULL;
ALTER TABLE chat_group ADD COLUMN announcement TEXT;
ALTER TABLE chat_group ADD COLUMN folder_path VARCHAR(512) DEFAULT NULL;