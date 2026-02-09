// guestbook.c - Fixed: Use cgiFormInteger() instead of cgiParamGet()
// Compile: gcc -Wall -std=c99 -O2 -o guestbook.cgi guestbook.c -lcgic -lsqlite3

#include <cgic.h>
#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define DB_FILE "guestbook.db"
#define PAGE_SIZE 10

// === MODEL ===
static sqlite3 *db = NULL;

static int init_db(void) {
    if (sqlite3_open(DB_FILE, &db) != SQLITE_OK) return -1;
    const char *sql = "CREATE TABLE IF NOT EXISTS entries ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "name TEXT NOT NULL, "
                      "message TEXT NOT NULL, "
                      "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)";
    return sqlite3_exec(db, sql, NULL, NULL, NULL);
}

static int insert_entry(const char *name, const char *message) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO entries (name, message) VALUES (?, ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) return -1;
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, message, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
    sqlite3_finalize(stmt);
    return rc;
}

static int get_total_count(int *count) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*) FROM entries";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) return -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return 0;
}

static int get_entries_paginated(char ***entries, int *count, int page) {
    sqlite3_stmt *stmt;
    int offset = (page - 1) * PAGE_SIZE;
    const char *sql = "SELECT name, message, timestamp, id FROM entries ORDER BY id DESC LIMIT ? OFFSET ?";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) return -1;
    
    sqlite3_bind_int(stmt, 1, PAGE_SIZE);
    sqlite3_bind_int(stmt, 2, offset);
    
    *count = 0;
    *entries = calloc(PAGE_SIZE, sizeof(char*));
    while (sqlite3_step(stmt) == SQLITE_ROW && *count < PAGE_SIZE) {
        const char *name = (const char*)sqlite3_column_text(stmt, 0);
        const char *msg = (const char*)sqlite3_column_text(stmt, 1);
        const char *ts = (const char*)sqlite3_column_text(stmt, 2);
        int id = sqlite3_column_int(stmt, 3);
        
        (*entries)[*count] = malloc(1024);
        snprintf((*entries)[*count], 1024, 
                "<li id='entry-%d'><strong><a href='?id=%d'>#%d %s</a></strong> "
                "<small>(%s)</small><br>%s</li>",
                id, id, id, name ? name : "Anonymous", ts ? ts : "just now", msg ? msg : "");
        (*count)++;
    }
    sqlite3_finalize(stmt);
    return 0;
}

static int get_entry_by_id(char **entry_html, int id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT name, message, timestamp FROM entries WHERE id = ? LIMIT 1";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL)) return -1;
    sqlite3_bind_int(stmt, 1, id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *name = (const char*)sqlite3_column_text(stmt, 0);
        const char *msg = (const char*)sqlite3_column_text(stmt, 1);
        const char *ts = (const char*)sqlite3_column_text(stmt, 2);
        *entry_html = malloc(1024);
        snprintf(*entry_html, 1024, 
                "<div style='max-width:600px;margin:2em auto;padding:2em;background:white;box-shadow:0 4px 8px rgba(0,0,0,0.1);'>"
                "<h2>Entry #%d</h2><p><strong>%s</strong> <small>(%s)</small></p>"
                "<p style='line-height:1.6;'>%s</p>"
                "<p><a href='guestbook.cgi'>&larr; Back to Guestbook</a></p></div>", 
                id, name ? name : "Anonymous", ts ? ts : "just now", msg ? msg : "");
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);
    return -1;
}

// === VIEW ===
static void render_form(void) {
    fprintf(cgiOut, "<form method='POST'>\n");
    fprintf(cgiOut, "<p><label>Name: <input type='text' name='name' maxlength='64' required></label></p>\n");
    fprintf(cgiOut, "<p><label>Message: <textarea name='message' maxlength='255' required></textarea></label></p>\n");
    fprintf(cgiOut, "<p><input type='submit' value='Sign Guestbook'></p>\n");
    fprintf(cgiOut, "</form>\n");
}

static void render_entries(int count, char **entries) {
    if (count == 0) {
        fprintf(cgiOut, "<p><em>No entries yet. Be the first!</em></p>\n");
        return;
    }
    fprintf(cgiOut, "<h3>Recent Entries:</h3><ul>\n");
    for (int i = 0; i < count; i++) {
        fprintf(cgiOut, "%s\n", entries[i]);
        free(entries[i]);
    }
    fprintf(cgiOut, "</ul>\n");
}

static void render_pagination(int page, int total_count) {
    int total_pages = (total_count + PAGE_SIZE - 1) / PAGE_SIZE;
    if (total_pages <= 1) return;
    
    fprintf(cgiOut, "<div style='text-align:center;margin:2em 0;'>");
    fprintf(cgiOut, "<div style='display:inline-block;'>");
    
    // Previous
    if (page > 1) {
        fprintf(cgiOut, "<a href='?page=%d' style='margin:0 5px;padding:8px 12px;border:1px solid #007cba;color:#007cba;text-decoration:none;'>← Prev</a>", page-1);
    }
    
    // Page numbers
    int start = (page > 3) ? page-2 : 1;
    int end = (page < total_pages-2) ? page+2 : total_pages;
    if (start > 1) {
        fprintf(cgiOut, "<a href='?page=1' style='margin:0 2px;padding:8px 12px;border:1px solid #ccc;color:#666;'>1</a>");
        if (start > 2) fprintf(cgiOut, "<span>...</span>");
    }
    
    for (int i = start; i <= end; i++) {
        if (i == page) {
            fprintf(cgiOut, "<span style='margin:0 5px;padding:8px 12px;background:#007cba;color:white;font-weight:bold;'>%d</span>", i);
        } else {
            fprintf(cgiOut, "<a href='?page=%d' style='margin:0 2px;padding:8px 12px;border:1px solid #ccc;'>%d</a>", i, i);
        }
    }
    
    if (end < total_pages) {
        if (end < total_pages-1) fprintf(cgiOut, "<span>...</span>");
        fprintf(cgiOut, "<a href='?page=%d' style='margin:0 2px;padding:8px 12px;border:1px solid #ccc;color:#666;'>%d</a>", total_pages, total_pages);
    }
    
    // Next
    if (page < total_pages) {
        fprintf(cgiOut, "<a href='?page=%d' style='margin:0 5px;padding:8px 12px;border:1px solid #007cba;color:#007cba;text-decoration:none;'>Next →</a>", page+1);
    }
    
    fprintf(cgiOut, "</div><p style='margin-top:1em;color:#666;'>Page %d of %d (Total: %d entries)</p>", page, total_pages, total_count);
    fprintf(cgiOut, "</div>\n");
}

// === CONTROLLER ===
int cgiMain(void) {
    cgiHeaderContentType("text/html");
    
    if (init_db()) {
        fprintf(cgiOut, "<h1>Error: Cannot initialize database</h1>");
        return 1;
    }
    
    // Parse GET parameters using CGIC functions
    int page = 1, entry_id = 0;
    cgiFormInteger("page", &page, 1);   // Default: page 1
    cgiFormInteger("id", &entry_id, 0); // Default: no entry
    
    if (page < 1) page = 1;
    
    // Handle POST first (takes precedence)
    char name[65] = {0}, message[257] = {0};
    if (cgiFormStringNoNewlines("name", name, sizeof(name)) == cgiFormSuccess &&
        cgiFormStringNoNewlines("message", message, sizeof(message)) == cgiFormSuccess &&
        name[0] && message[0]) {
        insert_entry(name, message);
        page = 1;  // Reset to first page after POST
    }
    
    // === API ENDPOINT: Single entry ===
    if (entry_id > 0) {
        char *single_entry = NULL;
        if (get_entry_by_id(&single_entry, entry_id) == 0) {
            fprintf(cgiOut, "<!DOCTYPE html><html><head><title>Entry #%d</title>", entry_id);
            fprintf(cgiOut, "<meta charset='utf-8'><style>*{font-family: dejavu sans mono;} body{max-width:800px;margin:2em auto;}</style></head><body>");
            fprintf(cgiOut, "%s", single_entry);
            fprintf(cgiOut, "</body></html>");
            free(single_entry);
        } else {
            fprintf(cgiOut, "<!DOCTYPE html><html><head><title>Not Found</title>"
                    "<meta charset='utf-8'><style>*{font-family: dejavu sans mono;} body{font-family:monospace;max-width:600px;margin:2em auto;}</style></head>"
                    "<body><h1>Entry #%d not found</h1><p><a href='guestbook.cgi'>&larr; Back to Guestbook</a></p></body></html>", entry_id);
        }
        sqlite3_close(db);
        return 0;
    }
    
    // === MAIN GUESTBOOK ===
    int total_count = 0;
    get_total_count(&total_count);
    
    char **entries = NULL;
    int count = 0;
    get_entries_paginated(&entries, &count, page);
    
    fprintf(cgiOut, "<!DOCTYPE html><html><head><title>Guestbook</title>\n");
    fprintf(cgiOut, "<meta charset='utf-8'><style>\n");
    fprintf(cgiOut, "* {font-family: dejavu sans mono;}\n");
    fprintf(cgiOut, "body{max-width:700px;margin:2em auto;padding:1em;background:#f5f5f5;}\n");
    fprintf(cgiOut, "form input,textarea{width:100%%;box-sizing:border-box;margin:5px 0;padding:10px;border:1px solid #ddd;border-radius:4px;}\n");
    fprintf(cgiOut, "textarea{height:80px;resize:none;}\n");
    fprintf(cgiOut, "ul{margin:1em 0;padding-left:2em;}\n");
    fprintf(cgiOut, "li{margin:1em 0;padding:1em;border-left:4px solid #007cba;background:white;border-radius:4px;}\n");
    fprintf(cgiOut, "</style></head><body>\n");
    fprintf(cgiOut, "<h1>🖋️ Guestbook (Page %d)</h1>\n", page);
    
    render_entries(count, entries);
    free(entries);
    
    render_pagination(page, total_count);
    
    fprintf(cgiOut, "<hr style='border:2px solid #007cba;margin:3em 0;'>\n");
    render_form();
    fprintf(cgiOut, "</body></html>\n");
    
    sqlite3_close(db);
    return 0;
}
