#pragma once

#include <QString>

// 协议 type 字符串的唯一登记处：新消息类型必须先在这里加，两端只引用不手写
namespace MsgType {

// 基础与连接
inline const QString kRegister      = QStringLiteral("register");
inline const QString kLogin         = QStringLiteral("login");
inline const QString kLoginResp     = QStringLiteral("login_resp");
inline const QString kHeartbeat     = QStringLiteral("heartbeat");
inline const QString kHeartbeatResp = QStringLiteral("heartbeat_resp");
inline const QString kUserList      = QStringLiteral("user_list");
// 阶段5 新增登记：自建群组
inline const QString kGroupCreate     = QStringLiteral("group_create");
inline const QString kGroupCreateResp = QStringLiteral("group_create_resp");
inline const QString kGroupList       = QStringLiteral("group_list");
inline const QString kGroupListResp   = QStringLiteral("group_list_resp");
inline const QString kGroupInvite     = QStringLiteral("group_invite");
inline const QString kGroupInviteResp = QStringLiteral("group_invite_resp");
// 阶段18 新增登记：群聊搜索与加入申请
inline const QString kGroupSearch          = QStringLiteral("group_search");
inline const QString kGroupSearchResp      = QStringLiteral("group_search_resp");
inline const QString kGroupJoinRequest     = QStringLiteral("group_join_request");
inline const QString kGroupJoinRequestResp = QStringLiteral("group_join_request_resp");
inline const QString kGroupJoinRequests    = QStringLiteral("group_join_requests");
inline const QString kGroupJoinRequestsResp= QStringLiteral("group_join_requests_resp");
inline const QString kGroupJoinAccept      = QStringLiteral("group_join_accept");
inline const QString kGroupJoinReject      = QStringLiteral("group_join_reject");
inline const QString kGroupJoinHandleResp  = QStringLiteral("group_join_handle_resp");
inline const QString kGroupJoinNotify      = QStringLiteral("group_join_notify");
inline const QString kGroupJoinResult      = QStringLiteral("group_join_result");
inline const QString kKick          = QStringLiteral("kick");
inline const QString kError         = QStringLiteral("error");
// 阶段11 新增登记：好友系统
inline const QString kFriendRequest     = QStringLiteral("friend_request");
inline const QString kFriendAccept      = QStringLiteral("friend_accept");
inline const QString kFriendList        = QStringLiteral("friend_list");
inline const QString kFriendListResp    = QStringLiteral("friend_list_resp");
inline const QString kFriendNotify      = QStringLiteral("friend_notify");
// 阶段18 新增登记：好友申请待处理列表与拒绝
inline const QString kFriendRequests     = QStringLiteral("friend_requests");
inline const QString kFriendRequestsResp = QStringLiteral("friend_requests_resp");
inline const QString kFriendReject       = QStringLiteral("friend_reject");
inline const QString kFriendDelete       = QStringLiteral("friend_delete");
inline const QString kFriendDeleteResp   = QStringLiteral("friend_delete_resp");
// 阶段15 新增登记：加好友页按用户名/昵称关键词搜索
inline const QString kUserSearch     = QStringLiteral("user_search");
inline const QString kUserSearchResp = QStringLiteral("user_search_resp");
// 阶段11 新增登记：个人资料（改昵称 / 换头像 / 个人主页）
inline const QString kProfileGet    = QStringLiteral("profile_get");
inline const QString kProfileUpdate = QStringLiteral("profile_update");
inline const QString kProfileResp   = QStringLiteral("profile_resp");
inline const QString kAvatarGet     = QStringLiteral("avatar_get");
inline const QString kAvatarResp    = QStringLiteral("avatar_resp");

// 聊天与文件
inline const QString kChatGroup     = QStringLiteral("chat_group");
inline const QString kChatPrivate   = QStringLiteral("chat_private");
inline const QString kChatBroadcast = QStringLiteral("chat_broadcast");
// 阶段3 新增登记：历史消息查询（协议 §4.3 未列，按规范先在此登记）
inline const QString kChatHistory     = QStringLiteral("chat_history");
inline const QString kChatHistoryResp = QStringLiteral("chat_history_resp");
inline const QString kChatRecall      = QStringLiteral("chat_recall");
inline const QString kChatRecallResp  = QStringLiteral("chat_recall_resp");
inline const QString kOfflineMsg    = QStringLiteral("offline_msg");
inline const QString kFileMeta      = QStringLiteral("file_meta");
inline const QString kFileChunk     = QStringLiteral("file_chunk");
inline const QString kFileDone      = QStringLiteral("file_done");
inline const QString kFileResp      = QStringLiteral("file_resp");
inline const QString kFileShared    = QStringLiteral("file_shared");   // 群文件已存入共享文件夹

// 群活动报名
inline const QString kActivityCreate      = QStringLiteral("activity_create");
inline const QString kActivityList        = QStringLiteral("activity_list");
inline const QString kActivityListResp    = QStringLiteral("activity_list_resp");
inline const QString kActivityDetail      = QStringLiteral("activity_detail");
inline const QString kActivityDetailResp  = QStringLiteral("activity_detail_resp");
inline const QString kActivitySignup      = QStringLiteral("activity_signup");
inline const QString kActivitySignupCancel= QStringLiteral("activity_signup_cancel");
inline const QString kActivityCancel      = QStringLiteral("activity_cancel");
inline const QString kActivityNotify      = QStringLiteral("activity_notify");
inline const QString kActivityEnd         = QStringLiteral("activity_end");

// 朋友圈
inline const QString kMomentPublish   = QStringLiteral("moment_publish");
inline const QString kMomentList      = QStringLiteral("moment_list");
inline const QString kMomentListResp  = QStringLiteral("moment_list_resp");
inline const QString kMomentLike      = QStringLiteral("moment_like");
inline const QString kMomentComment   = QStringLiteral("moment_comment");
inline const QString kMomentDelete    = QStringLiteral("moment_delete");
inline const QString kMomentNotify    = QStringLiteral("moment_notify");
// 阶段8 新增登记：图片按需拉取 + 各操作回执
inline const QString kMomentImage       = QStringLiteral("moment_image");
inline const QString kMomentImageResp   = QStringLiteral("moment_image_resp");
inline const QString kMomentLikeResp    = QStringLiteral("moment_like_resp");
inline const QString kMomentCommentResp = QStringLiteral("moment_comment_resp");
inline const QString kMomentCommentDelete     = QStringLiteral("moment_comment_delete");
inline const QString kMomentCommentDeleteResp = QStringLiteral("moment_comment_delete_resp");
inline const QString kMomentDeleteResp  = QStringLiteral("moment_delete_resp");

// 随机匹配
inline const QString kMatchStart   = QStringLiteral("match_start");
inline const QString kMatchCancel  = QStringLiteral("match_cancel");
inline const QString kMatchResult  = QStringLiteral("match_result");
inline const QString kMatchTimeout = QStringLiteral("match_timeout");
inline const QString kMatchEnd     = QStringLiteral("match_end");
inline const QString kMatchMessage = QStringLiteral("match_message");

// 语音
inline const QString kVoiceMeta  = QStringLiteral("voice_meta");
inline const QString kVoiceChunk = QStringLiteral("voice_chunk");
inline const QString kVoiceDone  = QStringLiteral("voice_done");
inline const QString kVoiceResp  = QStringLiteral("voice_resp");

// 阶段1 自检专用：冒充登录上报昵称；阶段2 起由 login 取代
inline const QString kHello = QStringLiteral("hello");

// 阶段11：朋友圈背景（按用户存服务端）
inline const QString kMomentBgGet    = QStringLiteral("moment_bg_get");
inline const QString kMomentBgResp   = QStringLiteral("moment_bg_resp");
inline const QString kMomentBgUpdate = QStringLiteral("moment_bg_update");

// 阶段11 新增登记：群资料与群生命周期
inline const QString kGroupInfoGet    = QStringLiteral("group_info_get");
inline const QString kGroupInfoResp   = QStringLiteral("group_info_resp");
inline const QString kGroupInfoUpdate = QStringLiteral("group_info_update");
inline const QString kGroupLeave      = QStringLiteral("group_leave");
inline const QString kGroupTransfer   = QStringLiteral("group_transfer");
inline const QString kGroupDissolve   = QStringLiteral("group_dissolve");
// 阶段11：群共享文件夹
inline const QString kGroupFilesList    = QStringLiteral("group_files_list");
inline const QString kGroupFilesResp    = QStringLiteral("group_files_resp");
inline const QString kGroupFileMkdir    = QStringLiteral("group_file_mkdir");
inline const QString kGroupFileDelete  = QStringLiteral("group_file_delete");
inline const QString kGroupFileDeleteResp = QStringLiteral("group_file_delete_resp");
inline const QString kGroupFileDownload   = QStringLiteral("group_file_download");
inline const QString kGroupFileChunk      = QStringLiteral("group_file_chunk");
inline const QString kGroupFileDownloadDone = QStringLiteral("group_file_download_done");
inline const QString kGroupAvatarGet  = QStringLiteral("group_avatar_get");
inline const QString kGroupAvatarResp = QStringLiteral("group_avatar_resp");

}
