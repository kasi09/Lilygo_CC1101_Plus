"""Constants for the LILYGO T-Embed CC1101 Plus integration."""

DOMAIN = "lilygo_t_embed"
NAME = "LILYGO T-Embed CC1101 Plus"
VERSION = "0.2.0"
MIN_FIRMWARE = (0, 4, 0)
CONF_BASE_TOPIC = "base_topic"
CONF_OFFLINE = "offline_setup"
STATE_TIMEOUT = 45
COMMAND_TIMEOUT = 8
PLATFORMS = ["sensor", "binary_sensor", "switch", "button", "light", "event"]
INPUT_EVENTS = (
    "encoder_left",
    "encoder_right",
    "encoder_click",
    "encoder_hold",
    "side_click",
    "side_hold",
)
