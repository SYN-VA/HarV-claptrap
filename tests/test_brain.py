from claptrap import protocol as P


def test_command_shapes():
    assert P.stand() == "STAND\n"
    assert P.stride(4, 600) == "STRIDE 4 600\n"
    assert P.turn(30) == "TURN 30\n"
    assert P.turn(-30) == "TURN -30\n"
    assert P.dance(2) == "DANCE 2\n"
    assert P.chirp(1) == "CHIRP 1\n"
    assert P.ok_ack("STRIDE") == "OK STRIDE\n"


def test_args_are_ints():
    assert P.stride(4.0, 600.0) == "STRIDE 4 600\n"


def test_frame_checksum_matches_uno_parser():
    assert P.frame([-30, 35], checksum=False) == "F -30 35\n"
    assert P.frame([-30, 35]) == "F -30 35 *5\n"
    assert P.frame([30, -30, 300]) == "F 30 -30 300 *44\n"


def test_frame_drops_checksum_when_disabled():
    assert "*" not in P.frame([1, 2, 3], checksum=False)
