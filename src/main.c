#include <assert.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tidy.h>
#include <tidybuffio.h>
#include <tidyenum.h>
#include <tidyplatform.h>

#define STATUS_ENDPOINT "https://www.passportindia.gov.in/AppOnlineProject/statusTracker/trackStatusInpNew"
#define ATTR_TARGET_FORM_ID "trackStatusForFileNoNew"
#define INITIAL_CAPACITY 8
#define JSON_INDENTATION 4

typedef struct {
    void **items;
    size_t capacity;
    size_t size;
} List;

int init_list(List *list) {
    if (list == NULL)
        return 1;

    list->size = 0;
    list->capacity = INITIAL_CAPACITY;
    list->items = malloc(sizeof(void *) * list->capacity);
    return list->items == NULL;
}

int append(List *list, void *item) {
    if (list == NULL)
        return 1;

    if (list->size == list->capacity) {
        list->capacity = list->capacity + list->capacity / 2;
        list->items = realloc(list->items, sizeof(void *) * list->capacity);
        if (list->items == NULL) {
            return 1;
        }
    }

    list->items[list->size++] = item;
    return 0;
}

void cleanup(void) { curl_global_cleanup(); }

size_t accumulate_content(char *in, size_t size, size_t nmemb, TidyBuffer *out) {
    size_t len = size * nmemb;
    tidyBufAppend(out, in, len);
    return len;
}

int extract_status(TidyDoc doc);

int main(int argc, const char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s fileno dob\n", argv[0]);
        return 1;
    }

    const char *file_no = argv[1];
    const char *dob = argv[2];
    TidyDoc tdoc;
    TidyBuffer docbuf = {0};

    atexit(cleanup);
    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        fprintf(stderr, "ERROR: curl_global_init() failed\n");
        exit(1);
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "ERROR: curl_easy_init() failed\n");
        exit(1);
    }

    curl_easy_setopt(curl, CURLOPT_URL, STATUS_ENDPOINT);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, accumulate_content);

    char body[120];
    snprintf(body, 120,
             "optStatus=Application_Status&fileNo=%s&applDob=%s&action:"
             "trackStatusForFileNoNew=Track Status",
             file_no, dob);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);

    tdoc = tidyCreate();
    tidyOptSetBool(tdoc, TidyForceOutput, yes);
    tidyOptSetInt(tdoc, TidyWrapLen, 4096);
    tidyOptSetBool(tdoc, TidyShowWarnings, no);
    tidyOptSetBool(tdoc, TidyShowInfo, no);
    tidyOptSetBool(tdoc, TidyQuiet, yes);
    tidyOptSetBool(tdoc, TidyHtmlOut, yes);
    tidyBufInit(&docbuf);

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &docbuf);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "ERROR: curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        tidyBufFree(&docbuf);
        tidyRelease(tdoc);
        exit(1);
    }

    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    if (status_code != 200) {
        fprintf(stderr,
                "ERROR: response status code is not 200 (OK), server responded "
                "with %ld\n",
                status_code);
        curl_easy_cleanup(curl);
        tidyBufFree(&docbuf);
        tidyRelease(tdoc);
        exit(1);
    }

    int err = tidyParseBuffer(tdoc, &docbuf);
    if (err >= 0) {
        err = tidyCleanAndRepair(tdoc);
        if (err >= 0) {
            err = tidyRunDiagnostics(tdoc);
        }
    }

    if (extract_status(tdoc)) {
        fprintf(stderr, "ERROR: failed to extract status\n");
        exit(1);
    }

    curl_easy_cleanup(curl);
    tidyBufFree(&docbuf);
    tidyRelease(tdoc);
    return 0;
}

void _find_tags(TidyNode root, const char *target, List *tags) {
    ctmbstr name = tidyNodeGetName(root);
    if (name && strcmp(name, target) == 0) {
        append(tags, (void *)root);
    }

    for (TidyNode child = tidyGetChild(root); child; child = tidyGetNext(child)) {
        _find_tags(child, target, tags);
    }
}

TidyNode *find_tags(TidyNode root, const char *name, size_t *count) {
    List tags;
    init_list(&tags);
    _find_tags(root, name, &tags);
    assert(count != NULL && "count is NULL");
    *count = tags.size;
    return (TidyNode *)realloc(tags.items, sizeof(void *) * tags.size);
}

const char *get_attribute(TidyNode tnode, const char *attribute) {
    if (tnode == NULL || attribute == NULL)
        return NULL;

    for (TidyAttr attr = tidyAttrFirst(tnode); attr; attr = tidyAttrNext(attr)) {
        ctmbstr name = tidyAttrName(attr);
        if (strcmp(name, attribute) == 0) {
            return tidyAttrValue(attr);
        }
    }

    return NULL;
}

void print_status(TidyDoc tdoc, TidyNode *nodes, size_t n) {
    printf("{");
    if (n == 0 || nodes == NULL) {
        printf("}\n");
        return;
    }

    TidyBuffer buf;
    tidyBufInit(&buf);

    bool prev = false;

    for (size_t i = 0; i < n; i++) {
        char *key = NULL;
        char *value = NULL;

        size_t m = 0;
        TidyNode *cells = find_tags(nodes[i], "td", &m);
        // TODO: extract expected number of elements
        if (m != 2)
            goto loopend;

        // extract key
        TidyNode child = tidyGetChild(cells[0]);
        if (!child || tidyNodeIsText(child) == no)
            goto loopend;
        tidyNodeGetValue(tdoc, child, &buf);
        key = buf.bp ? strdup((char *)buf.bp) : NULL;

        // extract value
        child = tidyGetChild(cells[1]);
        if (!child || tidyNodeIsText(child) == no)
            goto loopend;

        tidyNodeGetValue(tdoc, child, &buf);
        value = buf.bp ? strdup((const char *)buf.bp) : NULL;
        printf("%s\n%*c\"%s\": \"%s\"", prev ? "," : "", JSON_INDENTATION, ' ', key ? key : "", value ? value : "");
        prev = true;

    loopend:
        if (key)
            free(key);
        if (value)
            free(value);
        free(cells);
    }

    tidyBufFree(&buf);
    printf("\n}\n");
}

int extract_status(TidyDoc doc) {
    if (doc == NULL)
        return 0;

    size_t n = 0;
    TidyNode *forms = find_tags(tidyGetRoot(doc), "form", &n);
    TidyNode form = NULL;

    for (size_t i = 0; i < n; i++) {
        const char *value = get_attribute(forms[i], "id");
        if (strcmp(value, ATTR_TARGET_FORM_ID) == 0) {
            form = forms[i];
            break;
        }
    }

    // deallocate memory from the list
    free(forms);

    if (form == NULL) {
        fprintf(stderr, "ERROR: failed to extract form with id %s\n", ATTR_TARGET_FORM_ID);
        return 1;
    }

    TidyNode *tables = find_tags(form, "table", &n);
    // TODO: extract expected number of elements
    if (n != 2) {
        fprintf(stderr,
                "ERROR: website markup has changed, expected 2 <table>, "
                "discovered %zu tags instead\n",
                n);
        return 1;
    }

    TidyNode table = tables[1];
    free(tables);

    TidyNode *rows = find_tags(table, "tr", &n);
    // TODO: extract expected number of elements
    if (n != 11) {
        fprintf(stderr,
                "ERROR: website markup has changed, expected 11 <tr>, "
                "discovered %zu tags instead\n",
                n);
        return 1;
    }

    print_status(doc, rows, n);
    free(rows);

    return 0;
}
