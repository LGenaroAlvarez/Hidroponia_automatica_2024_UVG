package com.example.hydromaticmonitor;

import androidx.appcompat.app.AppCompatActivity;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.view.WindowManager;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.widget.ImageView;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    private static int SPLASH_SCREEN = 5000; // 5 seconds

    Animation topAnim, bottomAnim;

    ImageView logo_image;
    TextView app_name, slogan;

    @SuppressLint("MissingInflatedId")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setContentView(R.layout.activity_main);

        // Obtener animaciones definidas
        topAnim = AnimationUtils.loadAnimation(this,R.anim.top_animation);
        bottomAnim = AnimationUtils.loadAnimation(this,R.anim.bottom_animation);

        // Enlazar vistas
        logo_image = findViewById(R.id.splashLogo);
        app_name = findViewById(R.id.txt_appName);
        slogan = findViewById(R.id.txt_slogan);

        // Asignar animaciones
        logo_image.setAnimation(topAnim);
        app_name.setAnimation(bottomAnim);
        slogan.setAnimation(bottomAnim);

        // Cambiar de pantalla (actividad) a pantalla con datos
        new Handler().postDelayed(() -> {
            Intent intent = new Intent(MainActivity.this, Dashboard.class);
            startActivity(intent);
            finish();
        }, SPLASH_SCREEN);
    }
}