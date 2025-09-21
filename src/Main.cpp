#include <print>

#include "Dotenv.hpp"
#include "Game.h"
#include "Swoq.hpp"
#include "Vector2d.h"

using namespace Swoq::Interface;
using namespace Swoq;

int main(int /*argc*/, char** /*argv*/)
{
  // Load .env file
  load_dotenv();

  // Create a new GameConnection instance using env variables
  auto           user_id        = require_env_str("SWOQ_USER_ID");
  auto           user_name      = require_env_str("SWOQ_USER_NAME");
  auto           host           = require_env_str("SWOQ_HOST");
  auto           replays_folder = get_env_str("SWOQ_REPLAYS_FOLDER");
  GameConnection connection(user_id, user_name, host, replays_folder);

  // Start a new Game using env variables
  auto level        = get_env_int("SWOQ_LEVEL");
  auto seed         = get_env_int("SWOQ_SEED");
  auto start_result = connection.start(level, seed);
  if(!start_result)
  {
    std::println(std::cerr, "Failed to start game: {}", start_result.error());
    return -1;
  }
  auto& game = (*start_result);

  Bot::Game  botGame(connection, std::move(game));
  const auto result = botGame.Run();

  return result ? 0 : -1;
}
