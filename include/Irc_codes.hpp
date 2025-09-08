#ifndef IRC_CODES_HPP
# define IRC_CODES_HPP

namespace ft_codes {

// reply codes
enum {
	WELCOME        = 1,   // 001
	YOUR_HOST       = 2,   // 002
	CHANNEL_MODE_IS  = 324, // 324
	NO_TOPIC        = 331, // 331
	TOPIC          = 332, // 332
	TOPIC_WHO_TIME   = 333, // 333
	INVITING       = 341, // 341
	NAMES_REPLY       = 353, // 353
	END_OF_NAMES     = 366  // 366
};

// error codes
enum {
	NO_SUCH_NICK          = 401, // 401
	NO_SUCH_CHANNEL       = 403, // 403
	CANNOT_SEND_TO_CHAN    = 404, // 404
	NO_RECIPIENT         = 411, // 411
	NO_TEXT_TO_SEND        = 412, // 412
	NO_NICKNAME_GIVEN     = 431, // 431
	INVALID_NICKNAME    = 432, // 432  (RFC: ERR_ERRONEUSNICKNAME)
	NICKNAME_IN_USE       = 433, // 433
	NOT_ON_CHANNEL        = 442, // 442
	USER_ON_CHANNEL    = 443, // 443
	NOT_REGISTERED       = 451, // 451
	NEED_MORE_PARAMS      = 461, // 461
	ALREADY_REGISTERED   = 462, // 462
	PASSWORD_MISMATCH     = 464, // 464
	KEY_ALREADY_SET      = 467, // 467
	CHANNEL_IS_FULL       = 471, // 471
	UNKNOWN_MODE         = 472, // 472
	INVITE_ONLY_CHANNEL      = 473, // 473
	BANNED_FROM_CHANNEL      = 474, // 474
	BAD_CHANNEL_KEY       = 475, // 475
	CHAN_OP_PRIVS_NEEDED    = 482  // 482
};

}


#endif