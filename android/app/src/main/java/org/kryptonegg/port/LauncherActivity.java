package org.kryptonegg.port;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashSet;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Launcher. With the game data imported it starts the game at once; otherwise it asks the user
 * to pick their Krypton Egg folder or ZIP with the Storage Access Framework, copies the game
 * files (case-insensitive, any subfolder) into files/game, validates the required set and then
 * starts the game (docs/android/architecture.md, "Asset import").
 */
public class LauncherActivity extends Activity {
    private static final int PICK_FOLDER = 1;
    private static final int PICK_ZIP = 2;
    private static final int MAX_DEPTH = 4;

    private TextView status;
    private Button folderButton;
    private Button zipButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (GameData.isComplete(this)) {
            startGame();
            return;
        }
        buildUi();
    }

    private void startGame() {
        Intent intent = new Intent(this, GameActivity.class);
        startActivity(intent);
        finish();
    }

    private int dp(int value) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value,
                getResources().getDisplayMetrics());
    }

    private void buildUi() {
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER_HORIZONTAL);
        layout.setPadding(dp(24), dp(32), dp(24), dp(24));
        layout.setBackgroundColor(Color.rgb(16, 32, 74));

        TextView title = new TextView(this);
        title.setText(R.string.app_name);
        title.setTextColor(Color.WHITE);
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 28);
        title.setGravity(Gravity.CENTER);
        layout.addView(title);

        TextView text = new TextView(this);
        text.setText("This app runs the original 1994 game, but does not include its data files.\n\n"
                + "Choose the folder that contains your Krypton Egg files (KE_TIT.GIF, KE_MENU.GIF, "
                + "KE_LDCWC.TAB, ...), or a ZIP archive of it. The game files are copied into this "
                + "app's private storage; nothing else is read.");
        text.setTextColor(Color.rgb(220, 226, 240));
        text.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        text.setPadding(0, dp(16), 0, dp(16));
        layout.addView(text);

        folderButton = new Button(this);
        folderButton.setText("Choose folder");
        folderButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                startActivityForResult(intent, PICK_FOLDER);
            }
        });
        layout.addView(folderButton);

        zipButton = new Button(this);
        zipButton.setText("Choose ZIP");
        zipButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                intent.setType("*/*");
                intent.putExtra(Intent.EXTRA_MIME_TYPES,
                        new String[] { "application/zip", "application/x-zip-compressed",
                                       "application/octet-stream" });
                startActivityForResult(intent, PICK_ZIP);
            }
        });
        layout.addView(zipButton);

        status = new TextView(this);
        status.setTextColor(Color.rgb(255, 214, 120));
        status.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        status.setPadding(0, dp(16), 0, 0);
        layout.addView(status);

        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(Color.rgb(16, 32, 74));
        scroll.addView(layout);
        setContentView(scroll);
    }

    private void setBusy(final boolean busy, final String message) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                folderButton.setEnabled(!busy);
                zipButton.setEnabled(!busy);
                status.setText(message);
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) {
            return;
        }
        final Uri uri = data.getData();
        final boolean folder = requestCode == PICK_FOLDER;
        setBusy(true, "Copying game files...");
        new Thread(new Runnable() {
            @Override
            public void run() {
                importFrom(uri, folder);
            }
        }, "KE import").start();
    }

    /** Copy into files/game.import, validate, then replace files/game. */
    private void importFrom(Uri uri, boolean folder) {
        File target = GameData.dir(this);
        File staging = new File(getFilesDir(), "game.import");
        deleteTree(staging);
        if (!staging.mkdirs()) {
            setBusy(false, "Cannot create " + staging);
            return;
        }
        Set<String> copied = new HashSet<>();
        try {
            if (folder) {
                Uri tree = uri;
                String root = DocumentsContract.getTreeDocumentId(tree);
                copyTree(tree, root, staging, copied, 0);
            } else {
                copyZip(uri, staging, copied);
            }
        } catch (IOException | RuntimeException e) {
            deleteTree(staging);
            setBusy(false, "Import failed: " + e.getMessage());
            return;
        }
        String missing = GameData.missing(staging);
        if (!missing.isEmpty()) {
            deleteTree(staging);
            setBusy(false, (copied.isEmpty() ? "No Krypton Egg files found there." :
                    "Found " + copied.size() + " game files, but these are missing:\n" + missing)
                    + "\n\nChoose the folder (or ZIP) of the original game.");
            return;
        }
        deleteTree(target);
        if (!staging.renameTo(target)) {
            setBusy(false, "Cannot move the imported files into place.");
            return;
        }
        setBusy(true, "Imported " + copied.size() + " files. Starting...");
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                startGame();
            }
        });
    }

    private void copyTree(Uri tree, String documentId, File staging, Set<String> copied, int depth)
            throws IOException {
        ContentResolver resolver = getContentResolver();
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, documentId);
        String[] columns = { DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                             DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                             DocumentsContract.Document.COLUMN_MIME_TYPE };
        try (Cursor cursor = resolver.query(children, columns, null, null, null)) {
            if (cursor == null) {
                return;
            }
            while (cursor.moveToNext()) {
                String id = cursor.getString(0);
                String name = cursor.getString(1);
                String mime = cursor.getString(2);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                    if (depth < MAX_DEPTH) {
                        copyTree(tree, id, staging, copied, depth + 1);
                    }
                    continue;
                }
                String canonical = name == null ? null : GameData.canonical(name);
                if (canonical == null || copied.contains(canonical)) {
                    continue;
                }
                Uri document = DocumentsContract.buildDocumentUriUsingTree(tree, id);
                try (InputStream in = resolver.openInputStream(document)) {
                    if (in == null) {
                        throw new IOException("cannot open " + name);
                    }
                    copyStream(in, new File(staging, canonical));
                }
                copied.add(canonical);
            }
        }
    }

    private void copyZip(Uri uri, File staging, Set<String> copied) throws IOException {
        try (InputStream raw = getContentResolver().openInputStream(uri)) {
            if (raw == null) {
                throw new IOException("cannot open the archive");
            }
            try (ZipInputStream zip = new ZipInputStream(raw)) {
                ZipEntry entry;
                while ((entry = zip.getNextEntry()) != null) {
                    if (entry.isDirectory()) {
                        continue;
                    }
                    String canonical = GameData.canonical(entry.getName());
                    if (canonical == null || copied.contains(canonical)) {
                        continue;
                    }
                    copyStream(zip, new File(staging, canonical));
                    copied.add(canonical);
                }
            }
        }
    }

    private static void copyStream(InputStream in, File file) throws IOException {
        byte[] buffer = new byte[65536];
        try (OutputStream out = new FileOutputStream(file)) {
            int n;
            while ((n = in.read(buffer)) > 0) {
                out.write(buffer, 0, n);
            }
        }
    }

    private static void deleteTree(File file) {
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteTree(child);
            }
        }
        //noinspection ResultOfMethodCallIgnored
        file.delete();
    }
}
