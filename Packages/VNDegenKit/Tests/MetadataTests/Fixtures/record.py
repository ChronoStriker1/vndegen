#!/usr/bin/env python3
"""Re-records the fixtures in this folder from the live APIs.

Keys come from ~/.config/vndegen/steamgriddb.key and ~/.config/vndegen/igdb.env and never reach a fixture.
Responses are trimmed: long lists keep their first three entries, and Steam's HTML-heavy fields are dropped.
"""
import json
import pathlib
import urllib.parse
import urllib.request

HERE = pathlib.Path(__file__).parent
CONFIG = pathlib.Path.home() / ".config/vndegen"
SGDB_KEY = (CONFIG / "steamgriddb.key").read_text().strip()
IGDB = dict(line.split("=", 1) for line in (CONFIG / "igdb.env").read_text().split())
UA = {"User-Agent": "VNDegen fixture recorder"}


def fetch(url, body=None, headers=None, content_type="application/json"):
    data = body.encode() if isinstance(body, str) else body
    request = urllib.request.Request(url, data=data, headers={**UA, **(headers or {})})
    if data is not None:
        request.add_header("Content-Type", content_type)
    with urllib.request.urlopen(request) as response:
        return json.load(response)


def save(name, value):
    (HERE / f"{name}.json").write_text(json.dumps(value, indent=1, ensure_ascii=False) + "\n")
    print("recorded", name)


def vndb(endpoint, body):
    return fetch(f"https://api.vndb.org/kana/{endpoint}", json.dumps(body))


VN_DETAIL = ("title,description,released,rating,image.url,image.thumbnail,image.sexual,developers.name,"
             "tags.name,tags.rating,tags.category,tags.spoiler,screenshots.url,screenshots.thumbnail,screenshots.sexual")
save("vndb-search-ddlc", vndb("vn", {"filters": ["search", "=", "Doki Doki Literature Club"],
                                     "fields": "title,released,image.thumbnail", "results": 10}))
save("vndb-search-nosocks", vndb("vn", {"filters": ["search", "=", "No Socks RPG"],
                                        "fields": "title,released,image.thumbnail", "results": 10}))
vn = vndb("vn", {"filters": ["id", "=", "v21905"], "fields": VN_DETAIL})
vn["results"][0]["tags"] = sorted(vn["results"][0]["tags"], key=lambda t: -t["rating"])[:12]
vn["results"][0]["screenshots"] = vn["results"][0]["screenshots"][:3]
save("vndb-vn-v21905", vn)
save("vndb-release-v21905", vndb("release", {
    "filters": ["and", ["vn", "=", ["id", "=", "v21905"]], ["extlink", "=", "steam"]],
    "fields": "extlinks.name,extlinks.id", "results": 10}))


def steam_search(term):
    query = urllib.parse.urlencode({"term": term, "cc": "us", "l": "english"})
    return fetch(f"https://store.steampowered.com/api/storesearch/?{query}")


save("steam-search-ddlc", steam_search("Doki Doki Literature Club"))
save("steam-search-nosocks", steam_search("No Socks RPG"))
KEEP = {"type", "name", "steam_appid", "short_description", "developers", "publishers", "release_date", "genres",
        "metacritic", "header_image", "screenshots"}
for app in ("698780", "4929970"):
    details = fetch(f"https://store.steampowered.com/api/appdetails?appids={app}&cc=us&l=english")
    details[app]["data"] = {k: v for k, v in details[app]["data"].items() if k in KEEP}
    details[app]["data"]["screenshots"] = details[app]["data"].get("screenshots", [])[:3]
    save(f"steam-appdetails-{app}", details)
    input_json = json.dumps({"ids": [{"appid": int(app)}], "context": {"language": "english", "country_code": "US"},
                             "data_request": {"include_assets": True}}, separators=(",", ":"))
    save(f"steam-assets-{app}", fetch("https://api.steampowered.com/IStoreBrowseService/GetItems/v1/?"
                                      + urllib.parse.urlencode({"input_json": input_json})))


def sgdb(path):
    result = fetch(f"https://www.steamgriddb.com/api/v2/{path}", headers={"Authorization": f"Bearer {SGDB_KEY}"})
    if isinstance(result.get("data"), list):
        result["data"] = result["data"][:3]
    return result


save("sgdb-search-ddlc", sgdb("search/autocomplete/Doki%20Doki%20Literature%20Club"))
save("sgdb-steam-698780", sgdb("games/steam/698780"))
save("sgdb-steam-4929970", sgdb("games/steam/4929970"))
for game in ("21043", "5519261"):
    save(f"sgdb-grids-{game}", sgdb(f"grids/game/{game}?dimensions=600x900"))
    save(f"sgdb-headers-{game}", sgdb(f"grids/game/{game}?dimensions=920x430,460x215"))
    for kind in ("heroes", "logos", "icons"):
        save(f"sgdb-{kind}-{game}", sgdb(f"{kind}/game/{game}"))

grant = fetch("https://id.twitch.tv/oauth2/token", urllib.parse.urlencode(
    {"client_id": IGDB["IGDB_CLIENT_ID"], "client_secret": IGDB["IGDB_CLIENT_SECRET"], "grant_type": "client_credentials"}),
    content_type="application/x-www-form-urlencoded")
token = grant["access_token"]
save("igdb-token", {**grant, "access_token": "fixture-token"})


def igdb(body):
    return fetch("https://api.igdb.com/v4/games", body, content_type="text/plain",
                 headers={"Client-ID": IGDB["IGDB_CLIENT_ID"], "Authorization": f"Bearer {token}"})


GAME_FIELDS = ("fields name,summary,url,first_release_date,total_rating,genres.name,involved_companies.developer,"
               "involved_companies.publisher,involved_companies.company.name,external_games.external_game_source,"
               "external_games.uid,cover.image_id,artworks.image_id,screenshots.image_id;")
save("igdb-search-ddlc", igdb('search "Doki Doki Literature Club"; fields name,first_release_date,cover.image_id; limit 10;'))
save("igdb-search-nosocks", igdb('search "No Socks RPG"; fields name,first_release_date,cover.image_id; limit 10;'))
save("igdb-game-55935", igdb(f"{GAME_FIELDS} where id = 55935;"))
save("igdb-game-411478", igdb(f"{GAME_FIELDS} where id = 411478;"))

for fixture in HERE.glob("*.json"):
    text = fixture.read_text()
    assert SGDB_KEY not in text and token not in text and IGDB["IGDB_CLIENT_SECRET"] not in text, fixture
print("no key or token in any fixture")
