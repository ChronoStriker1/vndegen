// The classes of Epic Online Services for Godot (the eosg extension), without Epic Online Services. Some games ship it
// alongside GodotSteam and use it only in their Epic Games Store build, but their scripts name its classes everywhere,
// and a script that names a class Godot doesn't know fails to compile. These classes only make the names known. The
// rest are empty, and the IEOS singleton answers every call with nothing and has every signal, which never fires. The
// singleton's class has a name of its own, since GDScript checks members against a class it knows by the singleton's
// name, and fails to compile the script for each one missing. On an object of another class, a member it lacks only
// warns.
#pragma once

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "scene/main/multiplayer_peer.h"
#include "scene/main/node.h"

namespace vngd::eos {

// Set by vngd_start for the game about to start. The module registers the classes when it is set.
extern bool wanted;

#define VNGD_EOS_CLASS(m_name, m_base) \
	class m_name : public m_base { \
		GDCLASS(m_name, m_base); \
	};

VNGD_EOS_CLASS(EOSGActiveSession, RefCounted)
VNGD_EOS_CLASS(EOSGContinuanceToken, RefCounted)
VNGD_EOS_CLASS(EOSGFileTransferRequest, RefCounted)
VNGD_EOS_CLASS(EOSGLobbyDetails, RefCounted)
VNGD_EOS_CLASS(EOSGLobbyModification, RefCounted)
VNGD_EOS_CLASS(EOSGLobbySearch, RefCounted)
VNGD_EOS_CLASS(EOSGMultiplayerPeer, MultiplayerPeerExtension)
VNGD_EOS_CLASS(EOSGPacketPeerMediator, Node)
VNGD_EOS_CLASS(EOSGPlayerDataStorageFileTransferRequest, EOSGFileTransferRequest)
VNGD_EOS_CLASS(EOSGPresenceModification, RefCounted)
VNGD_EOS_CLASS(EOSGSessionDetails, RefCounted)
VNGD_EOS_CLASS(EOSGSessionModification, RefCounted)
VNGD_EOS_CLASS(EOSGSessionSearch, RefCounted)
VNGD_EOS_CLASS(EOSGTitleStorageFileTransferRequest, EOSGFileTransferRequest)
VNGD_EOS_CLASS(EOSGTransaction, RefCounted)

#undef VNGD_EOS_CLASS

class IEOSStandIn : public Object {
	GDCLASS(IEOSStandIn, Object);

protected:
	// Scripts reach the extension's signals as properties, to connect to them or await them.
	bool _get(const StringName &p_name, Variant &r_ret) const {
		IEOSStandIn *self = const_cast<IEOSStandIn *>(this);
		if (!self->has_signal(p_name)) {
			self->add_user_signal(MethodInfo(p_name));
		}
		r_ret = Signal(self, p_name);
		return true;
	}

public:
	// Games call some of its methods every frame, such as tick, so a missing one mustn't log an error each time.
	Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override {
		Variant result = Object::callp(p_method, p_args, p_argcount, r_error);
		if (r_error.error == Callable::CallError::CALL_ERROR_INVALID_METHOD) {
			r_error.error = Callable::CallError::CALL_OK;
		}
		return result;
	}
};

inline IEOSStandIn *start() {
	GDREGISTER_CLASS(IEOSStandIn);
	GDREGISTER_CLASS(EOSGActiveSession);
	GDREGISTER_CLASS(EOSGContinuanceToken);
	GDREGISTER_CLASS(EOSGFileTransferRequest);
	GDREGISTER_CLASS(EOSGLobbyDetails);
	GDREGISTER_CLASS(EOSGLobbyModification);
	GDREGISTER_CLASS(EOSGLobbySearch);
	GDREGISTER_CLASS(EOSGMultiplayerPeer);
	GDREGISTER_CLASS(EOSGPacketPeerMediator);
	GDREGISTER_CLASS(EOSGPlayerDataStorageFileTransferRequest);
	GDREGISTER_CLASS(EOSGPresenceModification);
	GDREGISTER_CLASS(EOSGSessionDetails);
	GDREGISTER_CLASS(EOSGSessionModification);
	GDREGISTER_CLASS(EOSGSessionSearch);
	GDREGISTER_CLASS(EOSGTitleStorageFileTransferRequest);
	GDREGISTER_CLASS(EOSGTransaction);
	IEOSStandIn *singleton = memnew(IEOSStandIn);
	Engine::get_singleton()->add_singleton(Engine::Singleton("IEOS", singleton));
	return singleton;
}

} // namespace vngd::eos
