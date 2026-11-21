from claptrap.brain import PersonalityEngine
from claptrap.chatbot import ClaptrapChat


def make(seed=1):
    engine = PersonalityEngine(seed=seed)
    return engine, ClaptrapChat(engine)


def test_routes_by_word_boundary():
    engine, chat = make()
    assert chat.route("you are a total genius") == "compliment"
    assert chat.route("you're useless") == "insult"
    assert chat.route("do a dance!") == "dance"
    assert chat.route("what do you do all day?") == "question"
    assert chat.route("hello claptrap") == "greeting"
    assert chat.route("just vibing") == "idle"


def test_yo_is_not_you():
    engine, chat = make()
    hi_chat = chat.route("yo")
    you_chat = chat.route("you are fine")
    assert hi_chat == "greeting"
    assert you_chat != "greeting"


def test_reply_returns_line_and_theatrics():
    engine, chat = make()
    line, theatrics = chat.reply("dance for me")
    assert isinstance(line, str) and line
    assert theatrics == ["salsa dancing x2 (hips: bolted on)"]
    assert chat.history[-2:] == [("you", "dance for me"), ("claptrap", line)]


def test_registered_moods_exist():
    engine, chat = make()
    for mood in ("compliment", "insult", "question"):
        line = engine.speak(mood)
        assert isinstance(line, str) and line
