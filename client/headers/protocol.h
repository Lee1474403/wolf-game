#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QString>

namespace Protocol {
namespace Request {
inline const QString JOIN = "JOIN";           // JOIN|<4位房间码>|<昵称>
inline const QString READY = "READY";         // READY|1 / READY|0
inline const QString START = "START";         // 仅房主
inline const QString VOTE = "VOTE";           // VOTE|<玩家编号>
inline const QString COPY = "COPY";
inline const QString VIEW_PLAYER = "VIEW_PLAYER";
inline const QString VIEW_TABLE = "VIEW_TABLE";
inline const QString ROB = "ROB";
inline const QString SWAP = "SWAP";
inline const QString DRINK = "DRINK";
inline const QString REVEAL = "REVEAL";
inline const QString PASS = "PASS";
}

namespace Notify {
inline const QString JOINED = "JOINED";
inline const QString JOIN_REJECTED = "JOIN_REJECTED";
inline const QString ROOM_STATUS = "ROOM_STATUS";
inline const QString READY_ACK = "READY_ACK";
inline const QString ROLE = "ROLE";
inline const QString PHASE = "PHASE";
inline const QString ACTION = "ACTION";
inline const QString NOTICE = "NOTICE";
inline const QString RESULT = "RESULT";
inline const QString GAME_RESET = "GAME_RESET";
inline const QString ROOM_DISBANDED = "ROOM_DISBANDED";
inline const QString ERROR = "ERROR";
}
}

#endif
