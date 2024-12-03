#include <signal.h>
#include <raylib.h>
#include <mupdf/fitz.h>

struct pdf {
	fz_context *ctx;
	fz_document *doc;
	const char *path;
	int npages;
	Texture2D *pages;
	int *reloaded;
	int last_reload;
	float zoom;
	float start;
	int page_height;
};

#define FPS 30

static bool got_signal = false;

void sig_handler(int signum) {
	(void) signum;
	got_signal = true;
	SetWindowFocused();
}

static Vector2 pdf_get_size(struct pdf *pdf, int p) {
	float gap = 0;
	if (p < 0) {
		p = 0;
		gap = 0.01;
	}

	fz_page *page = fz_load_page(pdf->ctx, pdf->doc, p);
	fz_rect r = fz_bound_page(pdf->ctx, page);
	int x = GetScreenWidth() * pdf->zoom;
	int y = x * (r.y1 - r.y0) / (r.x1 - r.x0);
	return (Vector2) {
		.x = x,
		.y = y + gap * y,
	};
}

static struct pdf pdf_open(const char *path) {
	struct pdf pdf = {0};
	pdf.path = path;
	pdf.zoom = 1;
	pdf.start = 0.0;
	pdf.last_reload = 1;

	pdf.ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	fz_register_document_handlers(pdf.ctx);

	fz_try (pdf.ctx) {
		pdf.doc = fz_open_document(pdf.ctx, path);
	} fz_catch (pdf.ctx) {
		fz_drop_context(pdf.ctx);
		return pdf;
	}

	pdf.npages = fz_count_pages(pdf.ctx, pdf.doc);
	pdf.pages = malloc(sizeof(Texture2D) * pdf.npages);
	memset(pdf.pages, 0, sizeof(Texture2D) * pdf.npages);
	pdf.reloaded = malloc(sizeof(int) * pdf.npages);
	memset(pdf.reloaded, 0, sizeof(int) * pdf.npages);

	pdf.page_height = pdf_get_size(&pdf, -1).y;

	return pdf;
}

static void pdf_close(struct pdf pdf) {
	if (!pdf.ctx || !pdf.doc || !pdf.path || !pdf.pages) {
		return;
	}

	fz_drop_document(pdf.ctx, pdf.doc);
	fz_drop_context(pdf.ctx);
	for (int i = 0; i < pdf.npages; i++) {
		UnloadTexture(pdf.pages[i]);
	}

	free(pdf.pages);
	free(pdf.reloaded);
}

static struct pdf pdf_reload_from_disk(struct pdf pdf) {
	const char *path = pdf.path;
	float start = pdf.start;
	float zoom = pdf.zoom;
	pdf_close(pdf);

	pdf = pdf_open(path);
	pdf.start = start;
	pdf.zoom = zoom;
	pdf.page_height = pdf_get_size(&pdf, -1).y;

	return pdf;
}

static int pdf_load_page(struct pdf *pdf, int p) {
	fz_page *page = fz_load_page(pdf->ctx, pdf->doc, p);
	fz_rect r = fz_bound_page(pdf->ctx, page);
	Vector2 pdf_size = pdf_get_size(pdf, p);
	fz_matrix ctm = fz_scale(pdf_size.x / (r.x1 - r.x0), pdf_size.y / (r.y1 - r.y0));
	fz_pixmap *pix = fz_new_pixmap_from_page(pdf->ctx, page, ctm, fz_device_rgb(pdf->ctx), 0);
	if (!pix) {
		return 1;
	}

	if (pix->n != 3) {
		fz_drop_pixmap(pdf->ctx, pix);
		return 1;
	}

	Image img = {
		.data = pix->samples,
		.width = pix->w,
		.height = pix->h,
		.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
		.mipmaps = 1
	};

	UnloadTexture(pdf->pages[p]);
	pdf->pages[p] = LoadTextureFromImage(img);
	fz_drop_pixmap(pdf->ctx, pix);

	return 0;
}

static void pdf_load_visible_pages(struct pdf *pdf) {
	int start_page = (int) pdf->start;
	int end_page = pdf->start + GetScreenHeight() / (float) pdf->page_height;
	for (int i = start_page; i <= end_page; i++) {
		if (i < 0 || i >= pdf->npages) {
			continue;
		}

		if (pdf->reloaded[i] == pdf->last_reload) {
			continue;
		}

		pdf->reloaded[i] = pdf->last_reload;
		pdf_load_page(pdf, i);
	}
}

static void pdf_reload_inc(struct pdf *pdf) {
	pdf->last_reload++;
	pdf->page_height = pdf_get_size(pdf, -1).y;
}

static void check_zoom(struct pdf *pdf) {
	if (IsKeyPressed(KEY_SLASH)) { // '-' on german keyboard
		pdf->zoom -= 0.1;
		pdf_reload_inc(pdf);
	} else if (IsKeyPressed(KEY_RIGHT_BRACKET)) { // '+' on german keyboard
		pdf->zoom += 0.1;
		pdf_reload_inc(pdf);
	}

	if (pdf->zoom < 0.2) {
		pdf->zoom = 0.2;
		pdf_reload_inc(pdf);
	}
}

static void check_start_pos(struct pdf *pdf) {
	pdf->start += (GetMouseWheelMoveV().y * -100) / pdf->page_height;
	float max_pdf_start = pdf->npages - (GetScreenHeight() / (float) pdf->page_height);
	if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP)) {
		pdf->start--;
	} else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN)) {
		pdf->start++;
	}

	int num = -1;
	if (IsKeyPressed(KEY_ZERO))  { num = 0; }
	if (IsKeyPressed(KEY_ONE))   { num = 1; }
	if (IsKeyPressed(KEY_TWO))   { num = 2; }
	if (IsKeyPressed(KEY_THREE)) { num = 3; }
	if (IsKeyPressed(KEY_FOUR))  { num = 4; }
	if (IsKeyPressed(KEY_FIVE))  { num = 5; }
	if (IsKeyPressed(KEY_SIX))   { num = 6; }
	if (IsKeyPressed(KEY_SEVEN)) { num = 7; }
	if (IsKeyPressed(KEY_EIGHT)) { num = 8; }
	if (IsKeyPressed(KEY_NINE))  { num = 9; }

	if (num != -1) {
		pdf->start = pdf->npages / 10.0 * num;
	}

	if (pdf->start < 0) {
		pdf->start = 0;
	} else if (pdf->start > max_pdf_start) {
		pdf->start = max_pdf_start;
	}
}

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Usage: %s <pdf-file>\n", argv[0]);
		return 1;
	}

	const char *file = argv[1];
	if (!FileExists(file)) {
		printf("File %s not found\n", file);
		return 1;
	}

	signal(SIGUSR1, sig_handler);

	SetTraceLogLevel(LOG_NONE);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(800, 600, "pdfview");
	SetTargetFPS(FPS);

	struct pdf pdf = pdf_open(argv[1]);

	int draw_reload_message = 0;
	while (!WindowShouldClose() && !IsKeyPressed(KEY_Q)) {
		if (got_signal) {
			got_signal = false;
			pdf = pdf_reload_from_disk(pdf);
			draw_reload_message = 5 * FPS;
			DisableEventWaiting();
		}

		if (IsWindowResized()) {
			pdf_reload_inc(&pdf);
		}

		check_start_pos(&pdf);
		check_zoom(&pdf);

		pdf_load_visible_pages(&pdf);

		BeginDrawing();
		ClearBackground(BLACK);

		int start_page = pdf.start;
		int end_page = pdf.start + GetScreenHeight() / (float) pdf.page_height;
		int x_start = (GetScreenWidth() - pdf.pages[start_page].width) / 2;
		int y_start = (int) (-pdf.start * pdf.page_height) % pdf.page_height;
		if (end_page >= pdf.npages) {
			end_page = pdf.npages - 1;
		}

		for (int i = start_page; i <= end_page; i++) {
			DrawTexture(pdf.pages[i], x_start, y_start + (i - start_page) * pdf.page_height, WHITE);
		}

		int fontsize = 20;
		const char *pagetext = TextFormat("page %d/%d", start_page + 1, pdf.npages);
		int pagetext_w = MeasureText(pagetext, fontsize);
		DrawRectangle(0, 0, pagetext_w + 10, fontsize + 10, BLACK);
		DrawText(pagetext, 5, 5, fontsize, WHITE);

		if (draw_reload_message > 0) {
			draw_reload_message--;
			const char *reload_text = "document reloaded";
			int reload_text_w = MeasureText(reload_text, fontsize);
			int x = GetScreenWidth() - reload_text_w - 10;
			int y = GetScreenHeight() - fontsize - 10;
			DrawRectangle(x, y, reload_text_w + 10, fontsize + 10, BLACK);
			DrawText(reload_text, x + 5, y + 5, fontsize, GREEN);
		} else {
			EnableEventWaiting();
		}

		EndDrawing();
	}

	pdf_close(pdf);
	return 0;
}
