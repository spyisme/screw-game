import os

from flask import Flask, jsonify, send_from_directory, current_app, abort

from routes.debug_routes import debug_bp
from routes.game_routes import game_bp
from routes.room_routes import room_bp


def create_app() -> Flask:
    app = Flask(__name__)

    app.register_blueprint(room_bp)
    app.register_blueprint(game_bp)
    app.register_blueprint(debug_bp)

    @app.get("/")
    def index():
        try:
            return send_from_directory(current_app.root_path, "final.zip", as_attachment=True)
        except Exception:
            abort(404)

    @app.get("/health")
    def health():
        return jsonify({"status": "ok"})

    return app


app = create_app()


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)
